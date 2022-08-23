%global _version 2.0.16
%global _release 3
%global is_systemd 1
%global enable_shimv2 1
%global enable_embedded 1

Name:      iSulad
Version:   %{_version}
Release:   %{_release}
Summary:   Lightweight Container Runtime Daemon
License:   Mulan PSL v2
URL:       isulad
Source:    iSulad-2.0.tar.gz
BuildRoot: {_tmppath}/iSulad-%{version}
ExclusiveArch:  x86_64 aarch64

%ifarch x86_64 aarch64
Provides:       libhttpclient.so()(64bit)
Provides:       libisula.so()(64bit)
Provides:       libisulad_img.so()(64bit)
%endif

%if 0%{?is_systemd}
# Systemd 230 and up no longer have libsystemd-journal
BuildRequires: pkgconfig(systemd)
Requires: systemd-units
%else
Requires(post): chkconfig
Requires(preun): chkconfig
# This is for /sbin/service
Requires(preun): initscripts
%endif

%if 0%{?enable_embedded}
BuildRequires: sqlite-devel
Requires:      sqlite
%endif

%if 0%{?enable_shimv2}
BuildRequires: lib-shim-v2-devel
Requires:      lib-shim-v2
%endif

BuildRequires: cmake gcc-c++ lxc-devel lcr-devel yajl-devel clibcni-devel
BuildRequires: grpc-plugins grpc-devel protobuf-devel
BuildRequires: libcurl-devel libarchive-devel device-mapper-devel
BuildRequires: http-parser-devel
BuildRequires: libselinux-devel libwebsockets-devel
BuildRequires: systemd-devel git

Requires:      lcr lxc clibcni
Requires:      grpc libcurl http-parser
Requires:      libselinux libwebsockets libarchive device-mapper
Requires:      systemd

%description
This is a umbrella project for gRPC-services based Lightweight Container
Runtime Daemon, written by C.

%prep
%autosetup -c -n iSulad-%{version}

%build
mkdir -p build
cd build
%cmake \
    -DDEBUG=ON \
    -DLIB_INSTALL_DIR=%{_libdir} \
    -DCMAKE_INSTALL_PREFIX=/usr \
%if 0%{?enable_shimv2}
    -DENABLE_SHIM_V2=ON \
%endif
    ../
%make_build

%install
rm -rf %{buildroot}
cd build
install -d $RPM_BUILD_ROOT/%{_libdir}
install -m 0644 ./src/libisula.so             %{buildroot}/%{_libdir}/libisula.so
install -m 0644 ./src/utils/http/libhttpclient.so  %{buildroot}/%{_libdir}/libhttpclient.so
install -m 0644 ./src/daemon/modules/image/libisulad_img.so   %{buildroot}/%{_libdir}/libisulad_img.so
chmod +x %{buildroot}/%{_libdir}/libisula.so
chmod +x %{buildroot}/%{_libdir}/libhttpclient.so
chmod +x %{buildroot}/%{_libdir}/libisulad_img.so

install -d $RPM_BUILD_ROOT/%{_libdir}/pkgconfig
install -m 0640 ./conf/isulad.pc              %{buildroot}/%{_libdir}/pkgconfig/isulad.pc

install -d $RPM_BUILD_ROOT/%{_bindir}
install -m 0755 ./src/isula                  %{buildroot}/%{_bindir}/isula
install -m 0755 ./src/isulad-shim            %{buildroot}/%{_bindir}/isulad-shim
install -m 0755 ./src/isulad                  %{buildroot}/%{_bindir}/isulad
chrpath -d ./src/isula
chrpath -d ./src/isulad-shim
chrpath -d ./src/isulad

install -d $RPM_BUILD_ROOT/%{_includedir}/isulad
install -m 0644 ../src/daemon/modules/api/image_api.h         %{buildroot}/%{_includedir}/isulad/image_api.h

install -d $RPM_BUILD_ROOT/%{_sysconfdir}/isulad
install -m 0640 ../src/contrib/config/daemon.json           %{buildroot}/%{_sysconfdir}/isulad/daemon.json
install -m 0440 ../src/contrib/config/daemon_constants.json %{buildroot}/%{_sysconfdir}/isulad/daemon_constants.json
install -m 0640 ../src/contrib/config/seccomp_default.json  %{buildroot}/%{_sysconfdir}/isulad/seccomp_default.json

install -d $RPM_BUILD_ROOT/%{_sysconfdir}/default/isulad
install -m 0640 ../src/contrib/config/config.json           %{buildroot}/%{_sysconfdir}/default/isulad/config.json
install -m 0640 ../src/contrib/config/systemcontainer_config.json           %{buildroot}/%{_sysconfdir}/default/isulad/systemcontainer_config.json
install -m 0550 ../src/contrib/sysmonitor/isulad-check.sh        %{buildroot}/%{_sysconfdir}/default/isulad/isulad-check.sh

mkdir -p $RPM_BUILD_ROOT/%{_sysconfdir}/sysmonitor/process
cp ../src/contrib/sysmonitor/isulad-monit $RPM_BUILD_ROOT/etc/sysmonitor/process

install -d $RPM_BUILD_ROOT/%{_sysconfdir}/default/isulad/hooks
install -m 0640 ../src/contrib/config/hooks/default.json %{buildroot}/%{_sysconfdir}/default/isulad/hooks/default.json

install -d $RPM_BUILD_ROOT/%{_sysconfdir}/sysconfig
install -p -m 0640 ../src/contrib/config/iSulad.sysconfig $RPM_BUILD_ROOT/%{_sysconfdir}/sysconfig/iSulad

%if 0%{?is_systemd}
install -d $RPM_BUILD_ROOT/%{_unitdir}
install -p -m 0640 ../src/contrib/init/isulad.service $RPM_BUILD_ROOT/%{_unitdir}/isulad.service
%else
install -d $RPM_BUILD_ROOT/%{_initddir}
install -p -m 0640 ../src/contrib/init/isulad.init $RPM_BUILD_ROOT/%{_initddir}/isulad.init
%endif

install -d $RPM_BUILD_ROOT/usr/share/bash-completion/completions
install -p -m 0644 ../src/contrib/completion/isula $RPM_BUILD_ROOT/usr/share/bash-completion/completions/isula
%clean
rm -rf %{buildroot}

%pre
# support update from lcrd to isulad, will remove in next version
if [ "$1" = "2" ]; then
%if 0%{?is_systemd}
systemctl stop lcrd
systemctl disable lcrd
if [ -e %{_sysconfdir}/isulad/daemon.json ];then
    sed -i 's#/etc/default/lcrd/hooks#/etc/default/isulad/hooks#g' %{_sysconfdir}/isulad/daemon.json
fi
%else
/sbin/chkconfig --del lcrd
%endif
fi

%post
source /usr/share/bash-completion/completions/isula

if ! getent group isula > /dev/null; then
    groupadd --system isula
fi

if [ "$1" = "1" ]; then
%if 0%{?is_systemd}
systemctl enable isulad
systemctl start isulad
%else
/sbin/chkconfig --add isulad
%endif
elif [ "$1" = "2" ]; then
%if 0%{?is_systemd}
# support update from lcrd to isulad, will remove in next version
if [ -e %{_unitdir}/lcrd.service.rpmsave ]; then
    mv %{_unitdir}/lcrd.service.rpmsave %{_unitdir}/isulad.service
    sed -i 's/lcrd/isulad/g' %{_unitdir}/isulad.service
fi
systemctl status isulad | grep 'Active:' | grep 'running'
if [ $? -eq 0 ]; then
  systemctl restart isulad
else
  systemctl start isulad
fi
%else
/sbin/service isulad status | grep 'Active:' | grep 'running'
if [ $? -eq 0 ]; then
  /sbin/service isulad restart
fi
%endif
fi

if ! getent group isula > /dev/null; then
    groupadd --system isula
fi

%preun
%if 0%{?is_systemd}
%systemd_preun isulad
%else
if [ $1 -eq 0 ] ; then
    /sbin/service isulad stop >/dev/null 2>&1
    /sbin/chkconfig --del isulad
fi
%endif

%postun
%if 0%{?is_systemd}
%systemd_postun_with_restart isulad
%else
if [ "$1" -ge "1" ] ; then
    /sbin/service isulad condrestart >/dev/null 2>&1 || :
fi
%endif

%files
%attr(0600,root,root) %{_sysconfdir}/sysmonitor/process/isulad-monit
%attr(0550,root,root) %{_sysconfdir}/default/isulad/isulad-check.sh
%defattr(0640,root,root,0750)
%{_sysconfdir}/isulad
%{_sysconfdir}/isulad/*
%{_sysconfdir}/default/*
%defattr(-,root,root,-)
%if 0%{?is_systemd}
%{_unitdir}/isulad.service
%attr(0640,root,root) %{_unitdir}/isulad.service
%else
%{_initddir}/isulad.init
%attr(0640,root,root) %{_initddir}/isulad.init
%endif
%{_includedir}/isulad/*
%attr(0755,root,root) %{_libdir}/pkgconfig
%attr(0640,root,root) %{_libdir}/pkgconfig/isulad.pc
%defattr(0755,root,root,0755)
%{_bindir}/*
%{_libdir}/*
%attr(0640,root,root) %{_sysconfdir}/sysconfig/iSulad
%attr(0640,root,root) %{_sysconfdir}/isulad/daemon.json

%config(noreplace,missingok) %{_sysconfdir}/sysconfig/iSulad
%config(noreplace,missingok) %{_sysconfdir}/isulad/daemon.json
%if 0%{?is_systemd}
%config(noreplace,missingok) %{_unitdir}/isulad.service
%else
%config(noreplace,missingok) %{_initddir}/isulad.init
%endif
/usr/share/bash-completion/completions/isula

%changelog
* Tue Oct 12 2020 wujing <wujing50@huawei.com> - 2.0.9-2
- Type:sync
- ID:NA
- SUG:NA
- DESC: pack daemon_constants.json

* Mon Oct 11 2020 wujing <wujing50@huawei.com> - 2.0.9-1
- Type:sync
- ID:NA
- SUG:NA
- DESC: upgrade version to 2.0.9-1

* Tue Sep 10 2020 openEuler Buildteam <buildteam@openeuler.org> - 2.0.5-20200910.140350.git72990229
- Type:enhancement
- ID:NA
- SUG:NA
- DESC: add chrpath

* Mon Aug 03 2020 openEuler Buildteam <buildteam@openeuler.org> - 2.0.3-20200803.130854.git0c7dc28a
- Type:enhancement
- ID:NA
- SUG:NA
- DESC: add debug packages
