#include "cri_runpodsandbox_service.h"

#include <sys/mount.h>
#include "isula_libutils/log.h"
#include "isula_libutils/host_config.h"
#include "isula_libutils/container_config.h"
#include "checkpoint_handler.h"
#include "utils.h"
#include "cri_helpers.h"
#include "cri_security_context.h"
#include "cri_constants.h"
#include "naming.h"
#include "service_container_api.h"
#include "cxxutils.h"
#include "network_namespace.h"
#include "cri_image_manager_service_impl.h"
#include "namespace.h"
namespace CRI
{
auto RunPodSandboxService::EnsureSandboxImageExists(const std::string &image, Errors &error) -> bool
{
    ImageManagerServiceImpl imageServiceImpl;
    ImageManagerService &imageService = imageServiceImpl;
    runtime::v1alpha2::ImageSpec imageRef;
    runtime::v1alpha2::AuthConfig auth;
    runtime::v1alpha2::ImageSpec imageSpec;
    Errors err;

    imageSpec.set_image(image);
    std::unique_ptr<runtime::v1alpha2::Image> imageStatus = imageService.ImageStatus(imageSpec, err);
    if (err.Empty()) {
        return true;
    }
    imageStatus.reset();

    imageRef.set_image(image);
    std::string outRef = imageService.PullImage(imageRef, auth, error);
    return !(!error.Empty() || outRef.empty());
}

void RunPodSandboxService::ApplySandboxLinuxOptions(const runtime::v1alpha2::LinuxPodSandboxConfig &lc,
                                                        host_config *hc, container_config *custom_config, Errors &error)
{
    CRISecurity::ApplySandboxSecurityContext(lc, custom_config, hc, error);
    if (error.NotEmpty()) {
        return;
    }

    if (!lc.cgroup_parent().empty()) {
        hc->cgroup_parent = util_strdup_s(lc.cgroup_parent().c_str());
    }
    int len = lc.sysctls_size();
    if (len <= 0) {
        return;
    }

    if (!lc.has_overhead()) {
        return;
    }
    if (!lc.has_resources()) {
        return;
    }

    if (len > LIST_SIZE_MAX) {
        error.Errorf("Too many sysctls, the limit is %d", LIST_SIZE_MAX);
        return;
    }
    hc->sysctls = (json_map_string_string *)util_common_calloc_s(sizeof(json_map_string_string));
    if (hc->sysctls == nullptr) {
        error.SetError("Out of memory");
        return;
    }

    auto iter = lc.sysctls().begin();
    while (iter != lc.sysctls().end()) {
        if (append_json_map_string_string(hc->sysctls, iter->first.c_str(), iter->second.c_str()) != 0) {
            error.SetError("Failed to append sysctl");
            return;
        }
        ++iter;
    }
}

void RunPodSandboxService::ApplySandboxResources(const runtime::v1alpha2::LinuxPodSandboxConfig * /*lc*/,
                                                     host_config *hc, Errors & /*error*/)
{
    hc->memory_swap = CRI::Constants::DefaultMemorySwap;
    hc->cpu_shares = CRI::Constants::DefaultSandboxCPUshares;
}

void RunPodSandboxService::SetHostConfigDefaultValue(host_config *hc)
{
    free(hc->network_mode);
    hc->network_mode = util_strdup_s(CRI::Constants::namespaceModeCNI.c_str());
}

void RunPodSandboxService::MakeSandboxIsuladConfig(const runtime::v1alpha2::PodSandboxConfig &c, host_config *hc,
                                                       container_config *custom_config, Errors &error)
{
    custom_config->labels = CRIHelpers::MakeLabels(c.labels(), error);
    if (error.NotEmpty()) {
        return;
    }
    if (append_json_map_string_string(custom_config->labels, CRIHelpers::Constants::CONTAINER_TYPE_LABEL_KEY.c_str(),
                                      CRIHelpers::Constants::CONTAINER_TYPE_LABEL_SANDBOX.c_str()) != 0) {
        error.SetError("Append container type into labels failed");
        return;
    }

    // Apply a container name label for infra container. This is used in summary v1.
    if (append_json_map_string_string(custom_config->labels,
                                      CRIHelpers::Constants::KUBERNETES_CONTAINER_NAME_LABEL.c_str(),
                                      CRIHelpers::Constants::POD_INFRA_CONTAINER_NAME.c_str()) != 0) {
        error.SetError("Append kubernetes container name into labels failed");
        return;
    }

    custom_config->annotations = CRIHelpers::MakeAnnotations(c.annotations(), error);
    if (error.NotEmpty()) {
        return;
    }
    if (append_json_map_string_string(custom_config->annotations,
                                      CRIHelpers::Constants::CONTAINER_TYPE_ANNOTATION_KEY.c_str(),
                                      CRIHelpers::Constants::CONTAINER_TYPE_ANNOTATION_SANDBOX.c_str()) != 0) {
        error.SetError("Append container type into annotation failed");
        return;
    }
    if (c.has_metadata()) {
        if (append_json_map_string_string(custom_config->annotations,
                                          CRIHelpers::Constants::SANDBOX_NAMESPACE_ANNOTATION_KEY.c_str(),
                                          c.metadata().namespace_().c_str()) != 0) {
            error.SetError("Append sandbox namespace into annotation failed");
            return;
        }
        if (append_json_map_string_string(custom_config->annotations,
                                          CRIHelpers::Constants::SANDBOX_NAME_ANNOTATION_KEY.c_str(),
                                          c.metadata().name().c_str()) != 0) {
            error.SetError("Append sandbox name into annotation failed");
            return;
        }
        if (append_json_map_string_string(custom_config->annotations,
                                          CRIHelpers::Constants::SANDBOX_UID_ANNOTATION_KEY.c_str(),
                                          c.metadata().uid().c_str()) != 0) {
            error.SetError("Append sandbox uid into annotation failed");
            return;
        }
        if (append_json_map_string_string(custom_config->annotations,
                                          CRIHelpers::Constants::SANDBOX_ATTEMPT_ANNOTATION_KEY.c_str(),
                                          std::to_string(c.metadata().attempt()).c_str()) != 0) {
            error.SetError("Append sandbox attempt into annotation failed");
            return;
        }
    }
    //add CRI log_directory
    if (!c.log_directory().empty()) {
        if (append_json_map_string_string(custom_config->annotations,
                                          CRIHelpers::Constants::SANDBOX_LOG_DIRECTORY_ANNOTATION_KEY.c_str(),
                                          c.log_directory().c_str()) != 0) {
            error.SetError("Append log_directory into annotation failed");
            return;
        }
    }
    if (!c.hostname().empty()) {
        custom_config->hostname = util_strdup_s(c.hostname().c_str());
    }

    SetHostConfigDefaultValue(hc);

    if (c.has_linux()) {
        ApplySandboxLinuxOptions(c.linux(), hc, custom_config, error);
        if (error.NotEmpty()) {
            return;
        }
    }

    hc->oom_score_adj = CRI::Constants::PodInfraOOMAdj;

    ApplySandboxResources(c.has_linux() ? &c.linux() : nullptr, hc, error);
    if (error.NotEmpty()) {
        return;
    }

    const char securityOptSep = '=';
    const ::runtime::v1alpha2::LinuxSandboxSecurityContext &context = c.linux().security_context();

    // Security Opts
    if (c.linux().has_security_context()) {
        std::vector<std::string> securityOpts = CRIHelpers::GetSecurityOpts(
                context.has_seccomp(), context.seccomp(), context.seccomp_profile_path(), securityOptSep, error);
        if (error.NotEmpty()) {
            error.Errorf("failed to generate security options for sandbox %s: %s", c.metadata().name().c_str(),
                         error.GetMessage().c_str());
            return;
        }
        if (!securityOpts.empty()) {
            char **tmp_security_opt = nullptr;

            if (securityOpts.size() > (SIZE_MAX / sizeof(char *)) - hc->security_opt_len) {
                error.Errorf("Out of memory");
                return;
            }
            size_t newSize = (hc->security_opt_len + securityOpts.size()) * sizeof(char *);
            size_t oldSize = hc->security_opt_len * sizeof(char *);
            int ret = util_mem_realloc((void **)(&tmp_security_opt), newSize, (void *)hc->security_opt, oldSize);
            if (ret != 0) {
                error.Errorf("Out of memory");
                return;
            }
            hc->security_opt = tmp_security_opt;
            for (const auto &securityOpt : securityOpts) {
                hc->security_opt[hc->security_opt_len] = util_strdup_s(securityOpt.c_str());
                hc->security_opt_len++;
            }
        }
    }
}

auto RunPodSandboxService::ParseCheckpointProtocol(runtime::v1alpha2::Protocol protocol) -> std::string
{
    switch (protocol) {
        case runtime::v1alpha2::UDP:
            return "udp";
        case runtime::v1alpha2::TCP:
        default:
            return "tcp";
    }
}

void RunPodSandboxService::ConstructPodSandboxCheckpoint(const runtime::v1alpha2::PodSandboxConfig &config,
                                                             CRI::PodSandboxCheckpoint &checkpoint)
{
    checkpoint.SetName(config.metadata().name());
    checkpoint.SetNamespace(config.metadata().namespace_());
    checkpoint.SetData(new CRI::CheckpointData);

    int len = config.port_mappings_size();
    for (int i = 0; i < len; i++) {
        CRI::PortMapping item;

        const runtime::v1alpha2::PortMapping &iter = config.port_mappings(i);
        item.SetProtocol(ParseCheckpointProtocol(iter.protocol()));
        item.SetContainerPort(iter.container_port());
        item.SetHostPort(iter.host_port());
        (checkpoint.GetData())->InsertPortMapping(item);
    }
    if (config.linux().security_context().namespace_options().network() == runtime::v1alpha2::NamespaceMode::NODE) {
        (checkpoint.GetData())->SetHostNetwork(true);
    }
}

container_create_request *RunPodSandboxService::PackCreateContainerRequest(
        const runtime::v1alpha2::PodSandboxConfig &config, const std::string &image, host_config *hostconfig,
        container_config *custom_config, const std::string &runtimeHandler, Errors &error)
{
    struct parser_context ctx = { OPT_GEN_SIMPLIFY, 0 };
    parser_error perror = nullptr;
    container_create_request *create_request =
            (container_create_request *)util_common_calloc_s(sizeof(*create_request));
    if (create_request == nullptr) {
        error.Errorf("Out of memory");
        return nullptr;
    }

    std::string sandboxName = CRINaming::MakeSandboxName(config.metadata());
    create_request->id = util_strdup_s(sandboxName.c_str());

    if (!runtimeHandler.empty()) {
        create_request->runtime = CRIHelpers::cri_runtime_convert(runtimeHandler.c_str());
        if (create_request->runtime == nullptr) {
            create_request->runtime = util_strdup_s(runtimeHandler.c_str());
        }
    }

    create_request->image = util_strdup_s(image.c_str());

    create_request->hostconfig = host_config_generate_json(hostconfig, &ctx, &perror);
    if (create_request->hostconfig == nullptr) {
        error.Errorf("Failed to generate host config json: %s", perror);
        goto error_out;
    }
    create_request->customconfig = container_config_generate_json(custom_config, &ctx, &perror);
    if (create_request->customconfig == nullptr) {
        error.Errorf("Failed to generate custom config json: %s", perror);
        goto error_out;
    }

    free(perror);
    return create_request;
error_out:
    free_container_create_request(create_request);
    free(perror);
    return nullptr;
}

container_create_request *
RunPodSandboxService::GenerateSandboxCreateContainerRequest(const runtime::v1alpha2::PodSandboxConfig &config,
                                                                const std::string &image, std::string &jsonCheckpoint,
                                                                const std::string &runtimeHandler, Errors &error)
{
    container_create_request *create_request = nullptr;
    host_config *hostconfig = nullptr;
    container_config *custom_config = nullptr;
    CRI::PodSandboxCheckpoint checkpoint;

    hostconfig = (host_config *)util_common_calloc_s(sizeof(host_config));
    if (hostconfig == nullptr) {
        error.SetError("Out of memory");
        goto error_out;
    }
    hostconfig->ipc_mode = util_strdup_s(SHARE_NAMESPACE_SHAREABLE);
    custom_config = (container_config *)util_common_calloc_s(sizeof(container_config));
    if (custom_config == nullptr) {
        error.SetError("Out of memory");
        goto error_out;
    }

    MakeSandboxIsuladConfig(config, hostconfig, custom_config, error);
    if (error.NotEmpty()) {
        ERROR("Failed to make sandbox config for pod %s: %s", config.metadata().name().c_str(), error.GetCMessage());
        error.Errorf("Failed to make sandbox config for pod %s: %s", config.metadata().name().c_str(),
                     error.GetCMessage());
        goto error_out;
    }

    // add checkpoint into annotations
    ConstructPodSandboxCheckpoint(config, checkpoint);
    jsonCheckpoint = CRIHelpers::CreateCheckpoint(checkpoint, error);
    if (error.NotEmpty()) {
        goto error_out;
    }

    if (append_json_map_string_string(custom_config->annotations, CRIHelpers::Constants::POD_CHECKPOINT_KEY.c_str(),
                                      jsonCheckpoint.c_str()) != 0) {
        error.SetError("Append checkpoint into annotations failed");
        goto error_out;
    }

    create_request = PackCreateContainerRequest(config, image, hostconfig, custom_config, runtimeHandler, error);
    if (create_request == nullptr) {
        error.SetError("Failed to pack create container request");
        goto error_out;
    }

    goto cleanup;
error_out:
    free_container_create_request(create_request);
    create_request = nullptr;
cleanup:
    free_host_config(hostconfig);
    free_container_config(custom_config);
    return create_request;
}

auto RunPodSandboxService::CreateSandboxContainer(const runtime::v1alpha2::PodSandboxConfig &config,
                                                      const std::string &image, std::string &jsonCheckpoint,
                                                      const std::string &runtimeHandler, Errors &error) -> std::string
{
    std::string response_id;
    container_create_request *create_request =
            GenerateSandboxCreateContainerRequest(config, image, jsonCheckpoint, runtimeHandler, error);
    if (error.NotEmpty()) {
        return response_id;
    }

    container_create_response *create_response = nullptr;
    if (m_cb->container.create(create_request, &create_response) != 0) {
        if (create_response != nullptr && (create_response->errmsg != nullptr)) {
            error.SetError(create_response->errmsg);
        } else {
            error.SetError("Failed to call create container callback");
        }
        goto cleanup;
    }
    response_id = create_response->id;
cleanup:
    free_container_create_request(create_request);
    free_container_create_response(create_response);
    return response_id;
}

void RunPodSandboxService::SetNetworkReady(const std::string &podSandboxID, bool ready, Errors &error)
{
    std::lock_guard<std::mutex> lockGuard(m_networkReadyLock);

    m_networkReady[podSandboxID] = ready;
}

void RunPodSandboxService::StartSandboxContainer(const std::string &response_id, Errors &error)
{
    container_start_request *start_request =
            (container_start_request *)util_common_calloc_s(sizeof(container_start_request));
    if (start_request == nullptr) {
        error.SetError("Out of memory");
        return;
    }
    start_request->id = util_strdup_s(response_id.c_str());
    container_start_response *start_response = nullptr;
    int ret = m_cb->container.start(start_request, &start_response, -1, nullptr, nullptr);
    if (ret != 0) {
        if (start_response != nullptr && (start_response->errmsg != nullptr)) {
            error.SetError(start_response->errmsg);
        } else {
            error.SetError("Failed to call start container callback");
        }
    }
    free_container_start_request(start_request);
    free_container_start_response(start_response);
}

void RunPodSandboxService::SetupSandboxFiles(const std::string &resolvPath,
                                                 const runtime::v1alpha2::PodSandboxConfig &config, Errors &error)
{
    if (resolvPath.empty()) {
        return;
    }
    std::vector<std::string> resolvContentStrs;

    /* set DNS options */
    int len = config.dns_config().searches_size();
    if (len > CRI::Constants::MAX_DNS_SEARCHES) {
        error.SetError("DNSOption.Searches has more than 6 domains");
        return;
    }

    std::vector<std::string> servers(config.dns_config().servers().begin(), config.dns_config().servers().end());
    if (!servers.empty()) {
        resolvContentStrs.push_back("nameserver " + CXXUtils::StringsJoin(servers, "\nnameserver "));
    }

    std::vector<std::string> searches(config.dns_config().searches().begin(), config.dns_config().searches().end());
    if (!searches.empty()) {
        resolvContentStrs.push_back("search " + CXXUtils::StringsJoin(searches, " "));
    }

    std::vector<std::string> options(config.dns_config().options().begin(), config.dns_config().options().end());
    if (!options.empty()) {
        resolvContentStrs.push_back("options " + CXXUtils::StringsJoin(options, " "));
    }

    if (!resolvContentStrs.empty()) {
        std::string resolvContent = CXXUtils::StringsJoin(resolvContentStrs, "\n") + "\n";
        if (util_write_file(resolvPath.c_str(), resolvContent.c_str(), resolvContent.size(),
                            DEFAULT_SECURE_FILE_MODE) != 0) {
            error.SetError("Failed to write resolv content");
        }
    }
}
auto RunPodSandboxService::GetSandboxKey(const container_inspect *inspect_data) -> std::string
{
    if (inspect_data == nullptr || inspect_data->network_settings == nullptr ||
        inspect_data->network_settings->sandbox_key == nullptr) {
        ERROR("Inspect data does not have network settings");
        return std::string("");
    }

    return std::string(inspect_data->network_settings->sandbox_key);
}

void RunPodSandboxService::GetSandboxNetworkInfo(const runtime::v1alpha2::PodSandboxConfig &config,
                                                     const std::string &jsonCheckpoint,
                                                     const container_inspect *inspect_data, std::string &sandboxKey,
                                                     std::map<std::string, std::string> &networkOptions,
                                                     std::map<std::string, std::string> &stdAnnos, Errors &error)
{
    if (config.linux().security_context().namespace_options().network() == runtime::v1alpha2::NamespaceMode::NODE) {
        return;
    }

    if (!namespace_is_cni(inspect_data->host_config->network_mode)) {
        error.Errorf("Network mode is neither host nor cni");
        ERROR("Network mode is neither host nor cni");
        return;
    }

    sandboxKey = GetSandboxKey(inspect_data);
    if (sandboxKey.size() == 0) {
        error.Errorf("Inspect data does not have sandbox key");
        ERROR("Inspect data does not have sandbox key");
        return;
    }

    CRIHelpers::ProtobufAnnoMapToStd(config.annotations(), stdAnnos);
    stdAnnos[CRIHelpers::Constants::POD_CHECKPOINT_KEY] = jsonCheckpoint;
    stdAnnos.insert(std::pair<std::string, std::string>(CRIHelpers::Constants::POD_SANDBOX_KEY, sandboxKey));

    networkOptions["UID"] = config.metadata().uid();
}

void RunPodSandboxService::SetupSandboxNetwork(const runtime::v1alpha2::PodSandboxConfig &config,
                                                   const std::string &response_id,
                                                   const container_inspect *inspect_data,
                                                   const std::map<std::string, std::string> &networkOptions,
                                                   const std::map<std::string, std::string> &stdAnnos,
                                                   std::string &network_settings_json, Errors &error)
{
    // Setup sandbox files
    if (config.has_dns_config() && inspect_data->resolv_conf_path != nullptr) {
        INFO("Over write resolv.conf: %s", inspect_data->resolv_conf_path);
        SetupSandboxFiles(inspect_data->resolv_conf_path, config, error);
        if (error.NotEmpty()) {
            ERROR("failed to setup sandbox files");
            return;
        }
    }
    // Do not invoke network plugins if in hostNetwork mode.
    if (config.linux().security_context().namespace_options().network() == runtime::v1alpha2::NamespaceMode::NODE) {
        return;
    }

    // Setup networking for the sandbox.
    m_pluginManager->SetUpPod(config.metadata().namespace_(), config.metadata().name(),
                              Network::DEFAULT_NETWORK_INTERFACE_NAME, response_id, stdAnnos, networkOptions,
                              network_settings_json, error);
    if (error.NotEmpty()) {
        ERROR("SetupPod failed: %s", error.GetCMessage());
        return;
    }
}

auto RunPodSandboxService::GenerateUpdateNetworkSettingsReqest(const std::string &id, const std::string &json,
                                                                   Errors &error)
        -> container_update_network_settings_request *
{
    if (json.empty()) {
        return nullptr;
    }

    container_update_network_settings_request *req = (container_update_network_settings_request *)util_common_calloc_s(
            sizeof(container_update_network_settings_request));
    if (req == nullptr) {
        error.Errorf("container update network settings request: Out of memory");
        return nullptr;
    }
    req->id = util_strdup_s(id.c_str());
    req->setting_json = util_strdup_s(json.c_str());

    return req;
}
auto RunPodSandboxService::GetNetworkReady(const std::string &podSandboxID, Errors &error) -> bool
{
    std::lock_guard<std::mutex> lockGuard(m_networkReadyLock);

    bool ready { false };
    auto iter = m_networkReady.find(podSandboxID);
    if (iter != m_networkReady.end()) {
        ready = iter->second;
    } else {
        error.Errorf("Do not find network: %s", podSandboxID.c_str());
    }

    return ready;
}
auto RunPodSandboxService::ClearCniNetwork(const std::string &realSandboxID, bool hostNetwork, const std::string &ns,
                                            const std::string &name, std::vector<std::string> &errlist,
                                            std::map<std::string, std::string> &stdAnnos, Errors &
                                            /*error*/) -> int
{
    Errors networkErr;
    container_inspect *inspect_data = nullptr;

    bool ready = GetNetworkReady(realSandboxID, networkErr);
    if (!hostNetwork && (ready || networkErr.NotEmpty())) {
        Errors pluginErr;

        // hostNetwork has indicated network mode which render host config unnecessary
        // so that with_host_config is set to be false.
        inspect_data = CRIHelpers::InspectContainer(realSandboxID, pluginErr, false);
        if (pluginErr.NotEmpty()) {
            ERROR("Failed to inspect container");
        }

        std::string netnsPath = GetSandboxKey(inspect_data);
        if (netnsPath.size() == 0) {
            ERROR("Failed to get network namespace path");
            return 0;
        }

        stdAnnos.insert(std::pair<std::string, std::string>(CRIHelpers::Constants::POD_SANDBOX_KEY, netnsPath));
        m_pluginManager->TearDownPod(ns, name, Network::DEFAULT_NETWORK_INTERFACE_NAME, realSandboxID, stdAnnos,
                                     pluginErr);
        if (pluginErr.NotEmpty()) {
            WARN("TearDownPod cni network failed: %s", pluginErr.GetCMessage());
            errlist.push_back(pluginErr.GetMessage());
        } else {
            INFO("TearDownPod cni network: success");
            SetNetworkReady(realSandboxID, false, pluginErr);
            if (pluginErr.NotEmpty()) {
                WARN("set network ready: %s", pluginErr.GetCMessage());
            }
            // umount netns when cni removed network successfully
            if (remove_network_namespace(netnsPath.c_str()) != 0) {
                ERROR("Failed to umount directory %s:%s", netnsPath.c_str(), strerror(errno));
            }
        }
    }
    free_container_inspect(inspect_data);
    return 0;
}
auto RunPodSandboxService::RunPodSandbox(const runtime::v1alpha2::PodSandboxConfig &config,
                                             const std::string &runtimeHandler, Errors &error) -> std::string
{
    std::string response_id;
    std::string jsonCheckpoint;
    std::string network_setting_json;
    std::string netnsPath;
    std::map<std::string, std::string> stdAnnos;
    std::map<std::string, std::string> networkOptions;
    container_inspect *inspect_data { nullptr };
    container_update_network_settings_request *ips_request { nullptr };
    container_update_network_settings_response *ips_response { nullptr };
    std::vector<std::string> errlist;

    if (m_cb == nullptr || m_cb->container.create == nullptr || m_cb->container.start == nullptr) {
        error.SetError("Unimplemented callback");
        return response_id;
    }

    // Step 1: Pull the image for the sandbox.
    const std::string &image = m_podSandboxImage;
    if (!EnsureSandboxImageExists(image, error)) {
        ERROR("Failed to pull sandbox image %s: %s", image.c_str(), error.NotEmpty() ? error.GetCMessage() : "");
        error.Errorf("Failed to pull sandbox image %s: %s", image.c_str(), error.NotEmpty() ? error.GetCMessage() : "");
        goto cleanup;
    }

    // Step 2: Create the sandbox container.
    response_id = CreateSandboxContainer(config, image, jsonCheckpoint, runtimeHandler, error);
    if (error.NotEmpty()) {
        goto cleanup;
    }

    // Step 3: Enable network
    SetNetworkReady(response_id, false, error);
    if (error.NotEmpty()) {
        WARN("disable network: %s", error.GetCMessage());
        error.Clear();
    }

    // Step 4: Inspect container
    inspect_data = CRIHelpers::InspectContainer(response_id, error, true);
    if (error.NotEmpty()) {
        goto cleanup;
    }
    if (inspect_data == nullptr || inspect_data->host_config == nullptr) {
        error.Errorf("Failed to retrieve inspect data");
        ERROR("Failed to retrieve inspect data");
        goto cleanup;
    }

    // Step 5: Get networking info
    GetSandboxNetworkInfo(config, jsonCheckpoint, inspect_data, netnsPath, networkOptions, stdAnnos, error);
    if (error.NotEmpty()) {
        goto cleanup;
    }

    // Step 6: Mount network namespace when network mode is cni
    if (namespace_is_cni(inspect_data->host_config->network_mode)) {
        if (prepare_network_namespace(netnsPath.c_str(), false, 0) != 0) {
            error.Errorf("Failed to prepare network namespace: %s", netnsPath.c_str());
            ERROR("Failed to prepare network namespace: %s", netnsPath.c_str());
            goto cleanup;
        }
    }

    // Step 7: Setup networking for the sandbox.
    SetupSandboxNetwork(config, response_id, inspect_data, networkOptions, stdAnnos, network_setting_json, error);
    if (error.NotEmpty()) {
        goto cleanup_ns;
    }

    // Step 8: Start the sandbox container.
    StartSandboxContainer(response_id, error);
    if (error.NotEmpty()) {
        goto cleanup_network;
    }

    // Step 9: Save network settings json to disk
    ips_request = GenerateUpdateNetworkSettingsReqest(response_id, network_setting_json, error);
    if (!error.NotEmpty()) {
        if (ips_request == nullptr) {
            goto cleanup;
        }

        if (m_cb->container.update_network_settings(ips_request, &ips_response) == 0) {
            goto cleanup;
        }
        if (ips_response != nullptr && ips_response->errmsg != nullptr) {
            error.SetError(ips_response->errmsg);
        } else {
            error.SetError("Failed to update container network settings");
        }
    }

cleanup_network:
    if (ClearCniNetwork(response_id,
                        config.linux().security_context().namespace_options().network() ==
                                runtime::v1alpha2::NamespaceMode::NODE,
                        config.metadata().namespace_(), config.metadata().name(), errlist, stdAnnos, error) != 0) {
        ERROR("Failed to clean cni network");
    }

cleanup_ns:
    if (namespace_is_cni(inspect_data->host_config->network_mode)) {
        if (remove_network_namespace(netnsPath.c_str()) != 0) {
            ERROR("Failed to remove network namespace: %s", netnsPath.c_str());
        }
    }

cleanup:
    if (error.Empty()) {
        SetNetworkReady(response_id, true, error);
        DEBUG("set %s ready", response_id.c_str());
        error.Clear();
    }

    free_container_inspect(inspect_data);
    free_container_update_network_settings_request(ips_request);
    free_container_update_network_settings_response(ips_response);
    return response_id;
}

}

