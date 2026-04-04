#!/bin/bash
set -e

VERSION="1.0.0"
RELEASE="1"

# Create RPM build directories
mkdir -p ~/rpmbuild/{BUILD,RPMS,SOURCES,SPECS,SRPMS}

# Create spec file
cat > ~/rpmbuild/SPECS/p2p-platform.spec << 'SPEC'
Name:           p2p-platform
Version:        1.0.0
Release:        1%{?dist}
Summary:        P2P Platform with STUN/TURN/Signaling/DID servers

License:        MIT
URL:            https://github.com/example/p2p-platform

BuildRequires:  cmake >= 3.20
BuildRequires:  gcc-c++
BuildRequires:  boost-devel >= 1.70
BuildRequires:  openssl-devel
Requires:       boost >= 1.70
Requires:       openssl

%description
P2P Platform providing STUN, TURN, Signaling, and DID services.

%install
mkdir -p %{buildroot}/usr/bin
mkdir -p %{buildroot}/usr/lib
mkdir -p %{buildroot}/usr/lib/systemd/system
mkdir -p %{buildroot}/etc/p2p-platform

# Copy binaries
cp build/src/servers/stun/stun_server %{buildroot}/usr/bin/
cp build/src/servers/relay/relay_server %{buildroot}/usr/bin/
cp build/src/servers/signaling/p2p-signaling-server %{buildroot}/usr/bin/
cp build/src/servers/did/did-server %{buildroot}/usr/bin/

# Copy libraries
cp build/src/servers/stun/libstun_server.dylib %{buildroot}/usr/lib/ || true
cp build/src/servers/relay/librelay_server.dylib %{buildroot}/usr/lib/ || true
cp build/src/servers/signaling/libp2p_signaling_server.dylib %{buildroot}/usr/lib/ || true
cp build/src/servers/did/libdid_service.dylib %{buildroot}/usr/lib/ || true

%files
/usr/bin/stun_server
/usr/bin/relay_server
/usr/bin/p2p-signaling-server
/usr/bin/did-server
/usr/lib/*.dylib

%changelog
* $(date "+%a %b %d %Y") Builder - 1.0.0-1
- Initial release
SPEC

echo "RPM spec created"
