/******************************************************************************
 * Copyright (c) Huawei Technologies Co., Ltd. 2017-2019. All rights reserved.
 * iSulad licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 * Author: tanyifeng
 * Create: 2017-11-22
 * Description: provide net plugin functions
 *********************************************************************************/

#include "network_plugin.h"
#include <memory>
#include <utility>
#include <vector>
#include <map>
#include <unistd.h>

#include <isula_libutils/auto_cleanup.h>
#include <isula_libutils/log.h>
#include <isula_libutils/utils_memory.h>

#include "utils_network.h"
#include "utils.h"
#include "cxxutils.h"
#include "sysctl_tools.h"
#include "cri_helpers.h"
#include "cri_runtime_service.h"
#include "cstruct_wrapper.h"

namespace Network {
static void run_modprobe(void * /*args*/)
{
    execlp("modprobe", "modprobe", "br-netfilter", nullptr);
}

static void runGetIP(void *cmdArgs)
{
    constexpr size_t ARGS_NUM { 14 };
    constexpr size_t CMD_ARGS_NUM { 4 };
    char *args[ARGS_NUM];
    char **tmpArgs = reinterpret_cast<char **>(cmdArgs);

    if (util_array_len((const char **)tmpArgs) != CMD_ARGS_NUM) {
        COMMAND_ERROR("need four args");
        exit(1);
    }

    if (asprintf(&(args[1]), "--net=%s", tmpArgs[1]) < 0) {
        COMMAND_ERROR("Out of memory");
        exit(1);
    }

    args[0] = util_strdup_s(tmpArgs[0]);
    args[2] = util_strdup_s("-F");
    args[3] = util_strdup_s("--");
    args[4] = util_strdup_s("ip");
    args[5] = util_strdup_s("-o");
    args[6] = util_strdup_s(tmpArgs[3]);
    args[7] = util_strdup_s("addr");
    args[8] = util_strdup_s("show");
    args[9] = util_strdup_s("dev");
    args[10] = util_strdup_s(tmpArgs[2]);
    args[11] = util_strdup_s("scope");
    args[12] = util_strdup_s("global");
    args[13] = nullptr;
    execvp(tmpArgs[0], args);
}

static std::string ParseIPFromLine(const char *line, const char *stdout_str)
{
    char *cIP { nullptr };
    char **fields { nullptr };
    struct ipnet *ipnet_val {
        nullptr
    };
    std::string ret;

    fields = util_string_split(line, ' ');
    if (fields == nullptr) {
        ERROR("Out of memory");
        goto out;
    }
    if (util_array_len((const char **)fields) < 4) {
        ERROR("Unexpected address output %s ", line);
        goto out;
    }

    if (util_parse_ipnet_from_str(fields[3], &ipnet_val) != 0) {
        ERROR("CNI failed to parse ip from output %s", stdout_str);
        goto out;
    }
    cIP = util_ip_to_string(ipnet_val->ip, ipnet_val->ip_len);
    if (cIP == nullptr) {
        ERROR("Out of memory");
        goto out;
    }

    ret = cIP;
out:
    free(cIP);
    util_free_ipnet(ipnet_val);
    util_free_array(fields);
    return ret;
}

static void GetOnePodIP(std::string nsenterPath, std::string netnsPath, std::string interfaceName, std::string addrType,
                        std::vector<std::string> &ips, Errors &error)
{
    char *stderr_str { nullptr };
    char *stdout_str { nullptr };
    char **lines { nullptr };
    char **args { nullptr };
    size_t i;

    args = (char **)util_common_calloc_s(sizeof(char *) * 5);
    if (args == nullptr) {
        error.SetError("Out of memory");
        return;
    }

    args[0] = util_strdup_s(nsenterPath.c_str());
    args[1] = util_strdup_s(netnsPath.c_str());
    args[2] = util_strdup_s(interfaceName.c_str());
    args[3] = util_strdup_s(addrType.c_str());
    if (!util_exec_cmd(runGetIP, args, nullptr, &stdout_str, &stderr_str)) {
        error.Errorf("Unexpected command output %s with error: %s", stdout_str, stderr_str);
        goto free_out;
    }

    DEBUG("get ip : %s", stdout_str);
    /* get ip from stdout str */
    lines = util_string_split(stdout_str, '\n');
    if (lines == nullptr) {
        error.SetError("Out of memory");
        goto free_out;
    }

    if (util_array_len((const char **)lines) == 0) {
        error.Errorf("Unexpected command output %s", stdout_str);
        goto free_out;
    }

    for (i = 0; i < util_array_len((const char **)lines); i++) {
        // ip string min length must bigger than 4
        if (lines[i] == nullptr || strlen(lines[i]) < 4) {
            continue;
        }
        std::string tIP = ParseIPFromLine(lines[i], stdout_str);
        if (tIP.empty()) {
            error.Errorf("Parse %s to ip failed", lines[i]);
            break;
        }
        ips.push_back(tIP);
    }

free_out:
    free(stdout_str);
    free(stderr_str);
    util_free_array(args);
    util_free_array(lines);
}

void GetPodIP(const std::string &nsenterPath, const std::string &netnsPath, const std::string &interfaceName,
              std::vector<std::string> &getIPs, Errors &error)
{
    Errors tmpErr;

    GetOnePodIP(nsenterPath, netnsPath, interfaceName, "-4", getIPs, tmpErr);
    if (tmpErr.NotEmpty()) {
        WARN("Get ipv4 failed: %s", tmpErr.GetCMessage());
    }

    GetOnePodIP(nsenterPath, netnsPath, interfaceName, "-6", getIPs, error);
    if (error.NotEmpty()) {
        WARN("Get ipv6 failed: %s", tmpErr.GetCMessage());
    }

    if (getIPs.size() > 0) {
        error.Clear();
        return;
    }

    if (tmpErr.NotEmpty()) {
        error.AppendError(tmpErr.GetMessage());
    }
}

void InitNetworkPlugin(std::vector<std::shared_ptr<NetworkPlugin>> *plugins, std::string networkPluginName,
                       std::string hairpinMode, std::string nonMasqueradeCIDR, int mtu,
                       std::shared_ptr<NetworkPlugin> *result, Errors &err)
{
    std::string allErr;

    if (networkPluginName.empty()) {
        DEBUG("network plugin name empty");
        *result = std::shared_ptr<NetworkPlugin>(new (std::nothrow) NoopNetworkPlugin);
        if (*result == nullptr) {
            ERROR("Out of memory");
            return;
        }
        (*result)->Init(hairpinMode, nonMasqueradeCIDR, mtu, err);
        return;
    }

    std::map<std::string, std::shared_ptr<NetworkPlugin>> pluginMap;

    for (auto it = plugins->begin(); it != plugins->end(); ++it) {
        std::string tmpName = (*it)->Name();
        // qualify plugin name
        if (pluginMap.find(tmpName) != pluginMap.end()) {
            allErr += ("Network plugin " + tmpName + "was registered more than once");
            continue;
        }

        pluginMap[tmpName] = *it;
    }

    if (pluginMap.find(networkPluginName) == pluginMap.end()) {
        allErr += ("Network plugin " + networkPluginName + "not found.");
        err.SetError(allErr);
        pluginMap.clear();
        return;
    }
    *result = pluginMap.find(networkPluginName)->second;

    (*result)->Init(hairpinMode, nonMasqueradeCIDR, mtu, err);
    if (err.NotEmpty()) {
        allErr += ("Network plugin " + networkPluginName + " failed init: " + err.GetMessage());
        err.SetError(allErr);
    } else {
        INFO("Loaded network plugin %s", networkPluginName.c_str());
    }

    pluginMap.clear();
}

const std::string &NetworkPluginConf::GetDockershimRootDirectory() const
{
    return m_dockershimRootDirectory;
}

void NetworkPluginConf::SetDockershimRootDirectory(const std::string &rootDir)
{
    m_dockershimRootDirectory = rootDir;
}

const std::string &NetworkPluginConf::GetPluginConfDir() const
{
    return m_pluginConfDir;
}

void NetworkPluginConf::SetPluginConfDir(const std::string &confDir)
{
    m_pluginConfDir = confDir;
}

const std::string &NetworkPluginConf::GetPluginBinDir() const
{
    return m_pluginBinDir;
}

void NetworkPluginConf::SetPluginBinDir(const std::string &binDir)
{
    m_pluginBinDir = binDir;
}

const std::string &NetworkPluginConf::GetPluginName() const
{
    return m_pluginName;
}

void NetworkPluginConf::SetPluginName(const std::string &name)
{
    m_pluginName = name;
}

const std::string &NetworkPluginConf::GetHairpinMode() const
{
    return m_hairpinMode;
}

void NetworkPluginConf::SetHairpinMode(const std::string &mode)
{
    m_hairpinMode = mode;
}

const std::string &NetworkPluginConf::GetNonMasqueradeCIDR() const
{
    return m_nonMasqueradeCIDR;
}

void NetworkPluginConf::SetNonMasqueradeCIDR(const std::string &cidr)
{
    m_nonMasqueradeCIDR = cidr;
}

int NetworkPluginConf::GetMTU()
{
    return m_mtu;
}

void NetworkPluginConf::SetMTU(int mtu)
{
    m_mtu = mtu;
}

const std::string &PodNetworkStatus::GetKind() const
{
    return m_kind;
}

void PodNetworkStatus::SetKind(const std::string &kind)
{
    m_kind = kind;
}

const std::string &PodNetworkStatus::GetAPIVersion() const
{
    return m_apiVersion;
}

void PodNetworkStatus::SetAPIVersion(const std::string &version)
{
    m_apiVersion = version;
}

const std::vector<std::string> &PodNetworkStatus::GetIPs() const
{
    return m_ips;
}

void PodNetworkStatus::SetIPs(std::vector<std::string> &ips)
{
    m_ips = ips;
}

void PluginManager::Lock(const std::string &fullPodName, Errors &error)
{
    if (pthread_mutex_lock(&m_podsLock) != 0) {
        error.SetError("Plugin manager lock failed");
        return;
    }
    auto iter = m_pods.find(fullPodName);
    PodLock *lock { nullptr };
    if (iter == m_pods.end()) {
        auto tmpLock = std::unique_ptr<PodLock>(new (std::nothrow) PodLock());
        if (tmpLock == nullptr) {
            error.SetError("Out of memory");
            if (pthread_mutex_unlock(&m_podsLock) != 0) {
                error.SetError("Plugin manager unlock failed");
            }
            return;
        }
        lock = tmpLock.get();
        m_pods[fullPodName] = std::move(tmpLock);
    } else {
        lock = iter->second.get();
    }
    lock->Increase();

    if (pthread_mutex_unlock(&m_podsLock) != 0) {
        error.SetError("Plugin manager unlock failed");
    }

    lock->Lock(error);
}

void PluginManager::Unlock(const std::string &fullPodName, Errors &error)
{
    if (pthread_mutex_lock(&m_podsLock) != 0) {
        error.SetError("Plugin manager lock failed");
        return;
    }

    auto iter = m_pods.find(fullPodName);
    PodLock *lock { nullptr };
    if (iter == m_pods.end()) {
        WARN("Unbalanced pod lock unref for %s", fullPodName.c_str());
        goto unlock;
    }
    lock = iter->second.get();
    if (lock->GetRefcount() == 0) {
        m_pods.erase(iter);
        WARN("Pod lock for %s still in map with zero refcount", fullPodName.c_str());
        goto unlock;
    }
    lock->Decrease();
    lock->Unlock(error);
    if (lock->GetRefcount() == 0) {
        m_pods.erase(iter);
    }
unlock:
    if (pthread_mutex_unlock(&m_podsLock) != 0) {
        error.SetError("Plugin manager unlock failed");
    }
}

std::string PluginManager::PluginName()
{
    if (m_plugin != nullptr) {
        return m_plugin->Name();
    }
    return "";
}

void PluginManager::Event(const std::string &name, std::map<std::string, std::string> &details)
{
    if (m_plugin != nullptr) {
        m_plugin->Event(name, details);
    }
}

void PluginManager::Status(Errors &error)
{
    if (m_plugin != nullptr) {
        m_plugin->Status(error);
    }
}

void PluginManager::SetUpPod(const std::string &ns, const std::string &name, const std::string &interfaceName,
                             const std::string &podSandboxID, const std::map<std::string, std::string> &annotations,
                             const std::map<std::string, std::string> &options, std::string &network_settings_json,
                             Errors &error)
{
    if (m_plugin == nullptr) {
        return;
    }

    Errors tmpErr;
    std::string fullName = name + "_" + ns;
    Lock(fullName, tmpErr);
    if (tmpErr.NotEmpty()) {
        error.AppendError(tmpErr.GetCMessage());
        return;
    }
    INFO("Calling network plugin %s to set up pod %s", m_plugin->Name().c_str(), fullName.c_str());

    m_plugin->SetUpPod(ns, name, interfaceName, podSandboxID, annotations, options, network_settings_json, tmpErr);
    if (tmpErr.NotEmpty()) {
        error.Errorf("NetworkPlugin %s failed to set up pod %s network: %s", m_plugin->Name().c_str(), fullName.c_str(),
                     tmpErr.GetCMessage());
    }

    tmpErr.Clear();
    Unlock(fullName, tmpErr);
    if (tmpErr.NotEmpty()) {
        error.AppendError(tmpErr.GetCMessage());
    }
}

void PluginManager::TearDownPod(const std::string &ns, const std::string &name, const std::string &interfaceName,
                                const std::string &podSandboxID, std::map<std::string, std::string> &annotations,
                                Errors &error)
{
    Errors tmpErr;
    std::string fullName = name + "_" + ns;
    Lock(fullName, tmpErr);
    if (tmpErr.NotEmpty()) {
        error.AppendError(tmpErr.GetCMessage());
        return;
    }
    if (m_plugin == nullptr) {
        goto unlock;
    }

    INFO("Calling network plugin %s to tear down pod %s", m_plugin->Name().c_str(), fullName.c_str());
    m_plugin->TearDownPod(ns, name, Network::DEFAULT_NETWORK_INTERFACE_NAME, podSandboxID, annotations, tmpErr);
    if (tmpErr.NotEmpty()) {
        error.Errorf("NetworkPlugin %s failed to teardown pod %s network: %s", m_plugin->Name().c_str(),
                     fullName.c_str(), tmpErr.GetCMessage());
    }
unlock:
    tmpErr.Clear();
    Unlock(fullName, tmpErr);
    if (tmpErr.NotEmpty()) {
        error.AppendError(tmpErr.GetCMessage());
    }
}

void NoopNetworkPlugin::Init(const std::string &hairpinMode, const std::string &nonMasqueradeCIDR, int mtu,
                             Errors &error)
{
    char *stderr_str { nullptr };
    char *stdout_str { nullptr };
    int ret;
    char *err { nullptr };

    if (!util_exec_cmd(run_modprobe, nullptr, nullptr, &stdout_str, &stderr_str)) {
        WARN("exec failed: [%s], [%s]", stdout_str, stderr_str);
    }

    ret = set_sysctl(SYSCTL_BRIDGE_CALL_IPTABLES.c_str(), 1, &err);
    if (ret != 0) {
        WARN("can't set sysctl %s: 1, err: %s", SYSCTL_BRIDGE_CALL_IPTABLES.c_str(), err);
        free(err);
        err = nullptr;
    }

    ret = get_sysctl(SYSCTL_BRIDGE_CALL_IP6TABLES.c_str(), &err);
    if (ret != 1) {
        free(err);
        err = nullptr;
        ret = set_sysctl(SYSCTL_BRIDGE_CALL_IP6TABLES.c_str(), 1, &err);
        if (ret != 0) {
            WARN("can't set sysctl %s: 1, err: %s", SYSCTL_BRIDGE_CALL_IP6TABLES.c_str(), err);
        }
    }

    free(err);
    free(stderr_str);
    free(stdout_str);
}

void NoopNetworkPlugin::Event(const std::string &name, std::map<std::string, std::string> &details)
{
    return;
}

const std::string &NoopNetworkPlugin::Name() const
{
    return DEFAULT_PLUGIN_NAME;
}

std::map<int, bool> *NoopNetworkPlugin::Capabilities()
{
    std::map<int, bool> *ret { new (std::nothrow) std::map<int, bool> };
    return ret;
}

void NoopNetworkPlugin::SetUpPod(const std::string &ns, const std::string &name, const std::string &interfaceName,
                                 const std::string &podSandboxID, const std::map<std::string, std::string> &annotations,
                                 const std::map<std::string, std::string> &options, std::string &network_settings_json,
                                 Errors &error)
{
    __isula_auto_free parser_error jerr { nullptr };
    __isula_auto_free char *setting_json { nullptr };

    // Even if cni plugin is not configured, noop network plugin will generate network_settings_json for executor to update
    // Since in CRI_API_V1, sandbox key is generated in sandbox, excutor will not be able to get it if network_setting is no updated.
    if (error.NotEmpty()) {
        return;
    }

    auto iter = annotations.find(CRIHelpers::Constants::POD_SANDBOX_KEY);
    if (iter == annotations.end()) {
        ERROR("Failed to find sandbox key from annotations");
        return;
    }
    const std::string netnsPath = iter->second;
    if (netnsPath.length() == 0) {
        ERROR("Failed to get network namespace path");
        return;
    }

    container_network_settings *network_settings = static_cast<container_network_settings *>
                                                   (util_common_calloc_s(sizeof(container_network_settings)));
    if (network_settings == nullptr) {
        ERROR("Out of memory");
        error.SetError("Out of memory");
        return;
    }
    auto settingsWarpper = std::unique_ptr<CStructWrapper<container_network_settings>>(new
                                                                                       CStructWrapper<container_network_settings>(network_settings, free_container_network_settings));

    network_settings->sandbox_key = util_strdup_s(netnsPath.c_str());

    setting_json = container_network_settings_generate_json(network_settings, nullptr, &jerr);
    if (setting_json == nullptr) {
        error.Errorf("Get network settings json err:%s", jerr);
        return;
    }

    network_settings_json = std::string(setting_json);
}

void NoopNetworkPlugin::TearDownPod(const std::string &ns, const std::string &name, const std::string &interfaceName,
                                    const std::string &podSandboxID,
                                    const std::map<std::string, std::string> &annotations, Errors &error)
{
    return;
}

void NoopNetworkPlugin::Status(Errors &error)
{
    return;
}

const std::string &GetInterfaceName()
{
    return DEFAULT_NETWORK_INTERFACE_NAME;
}

static void runGetNetworkStats(void *args)
{
    constexpr size_t ARGS_LEN { 10 };
    char **tmp_args = reinterpret_cast<char **>(args);

    if (util_array_len((const char **)tmp_args) != ARGS_LEN) {
        COMMAND_ERROR("Get network Stats need %lu args", ARGS_LEN);
        exit(1);
    }

    execvp(tmp_args[0], tmp_args);
}

static void ParseOneLineNetworkStats(std::string &headerLine, std::string &valueLine,
                                     uint64_t &bytesValue, uint64_t &errorsValue, Errors &error)
{
    if (headerLine.length() == 0 || valueLine.length() == 0) {
        error.Errorf("Invalid header line %s or value line %s", headerLine.c_str(), valueLine.c_str());
        return;
    }

    size_t bytesColumn = SIZE_MAX;
    size_t errorsColumn = SIZE_MAX;
    auto split = CXXUtils::SplitDropEmpty(headerLine, ' ');
    for (size_t i = 1; i < split.size(); i++) {
        if (split[i] == "bytes") {
            bytesColumn = i - 1;
            continue;
        }
        if (split[i] == "errors") {
            errorsColumn = i - 1;
            continue;
        }
    }
    if (bytesColumn >= split.size() || errorsColumn >= split.size()) {
        error.Errorf("Invalid header line: %s", headerLine.c_str());
        return;
    }

    split = CXXUtils::SplitDropEmpty(valueLine, ' ');
    if (bytesColumn >= split.size() || errorsColumn >= split.size()) {
        error.Errorf("Invalid value line: %s", valueLine.c_str());
        return;
    }

    if (util_safe_uint64(split[bytesColumn].c_str(), &bytesValue) != 0) {
        error.Errorf("Failed to convert %s to uint64", split[bytesColumn].c_str());
        return;
    }
    if (util_safe_uint64(split[errorsColumn].c_str(), &errorsValue) != 0) {
        error.Errorf("Failed to convert %s to uint64", split[errorsColumn].c_str());
        return;
    }
}

// Parse result of command: ip -s link show dev XXX
// for example
// 6: ens3: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc mq state UP mode DEFAULT group default qlen 1000
//    link/ether 00:15:5d:c4:29:0d brd ff:ff:ff:ff:ff:ff
//    RX: bytes  packets  errors  dropped overrun mcast
//    2686258    9336     0       0       0       7721
//    TX: bytes  packets  errors  dropped carrier collsns
//    175961     1097     0       0       0       0
//    altname enp0s3
static void ParseNetworkStats(const std::string &stdoutString, struct NetworkInterfaceStats &stats, Errors &error)
{
    if (stdoutString.length() == 0) {
        error.SetError("None command output");
        return;
    }

    size_t rxLine = SIZE_MAX;
    size_t txLine = SIZE_MAX;
    auto lines = CXXUtils::SplitDropEmpty(stdoutString, '\n');
    for (size_t i = 0; i < lines.size(); i++) {
        std::string trim = CXXUtils::StringTrim(lines[i]);
        if (trim.rfind("RX:", 0) == 0) {
            rxLine = i;
            continue;
        }
        if (trim.rfind("TX:", 0) == 0) {
            txLine = i;
            continue;
        }
    }

    if (rxLine >= lines.size() - 1 || txLine >= lines.size() - 1) {
        error.Errorf("Unexpected command output %s", stdoutString.c_str());
        return;
    }

    uint64_t rxBytes, rxErrors, txBytes, txErrors;
    ParseOneLineNetworkStats(lines[rxLine], lines[rxLine + 1], rxBytes, rxErrors, error);
    if (error.NotEmpty()) {
        return;
    }
    ParseOneLineNetworkStats(lines[txLine], lines[txLine + 1], txBytes, txErrors, error);
    if (error.NotEmpty()) {
        return;
    }

    stats.rxBytes = rxBytes;
    stats.rxErrors = rxErrors;
    stats.txBytes = txBytes;
    stats.txErrors = txErrors;
}

void GetPodNetworkStats(const std::string &nsenterPath, const std::string &netnsPath, const std::string &interfaceName,
                        struct NetworkInterfaceStats &stats, Errors &error)
{
    Errors tmpErr;
    size_t i = 0;
    constexpr size_t ARGS_LEN { 10 };
    char **args = { 0 };
    char *stdoutString { nullptr };
    char *stderrString { nullptr };

    args = (char **)util_smart_calloc_s(sizeof(char *), ARGS_LEN + 1);
    if (args == nullptr) {
        error.SetError("Out of memory");
        return;
    }

    // join command args: nsenter --net=XXX -F -- ip -s link show dev XXX
    args[i++] = util_strdup_s(nsenterPath.c_str());
    args[i++] = util_strdup_s((std::string("--net=") + netnsPath).c_str());
    args[i++] = util_strdup_s("-F");
    args[i++] = util_strdup_s("--");
    args[i++] = util_strdup_s("ip");
    args[i++] = util_strdup_s("-s");
    args[i++] = util_strdup_s("link");
    args[i++] = util_strdup_s("show");
    args[i++] = util_strdup_s("dev");
    args[i++] = util_strdup_s(interfaceName.c_str());
    args[i++] = nullptr;

    if (!util_exec_cmd(runGetNetworkStats, args, nullptr, &stdoutString, &stderrString)) {
        error.Errorf("Unexpected command output %s with error: %s", stdoutString, stderrString);
        goto free_out;
    }

    ParseNetworkStats(std::string(stdoutString), stats, tmpErr);
    if (tmpErr.NotEmpty()) {
        error.AppendError(tmpErr.GetMessage());
    }
    stats.name = interfaceName;

free_out:
    free(stdoutString);
    free(stderrString);
    util_free_array(args);
}

} // namespace Network
