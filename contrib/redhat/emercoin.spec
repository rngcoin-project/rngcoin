Name:           rngcoin
Version:        0.6.3
Release:        1%{?dist}
Summary:        Rngcoin Wallet
Group:          Applications/Internet
Vendor:         Rngcoin
License:        GPLv3
URL:            https://rng-coin.io
Source0:        %{name}-%{version}.tar.gz
BuildRoot:      %(mktemp -ud %{_tmppath}/%{name}-%{version}-%{release}-XXXXXX)
BuildRequires:  autoconf automake libtool gcc-c++ openssl-devel >= 1:1.0.2d libdb4-devel libdb4-cxx-devel miniupnpc-devel boost-devel boost-static
Requires:       openssl >= 1:1.0.2d libdb4 libdb4-cxx miniupnpc logrotate

%description
Rngcoin Wallet

%prep
%setup -q

%build
./autogen.sh
./configure
make

%install
%{__rm} -rf $RPM_BUILD_ROOT
%{__mkdir} -p $RPM_BUILD_ROOT%{_bindir} $RPM_BUILD_ROOT/etc/rngcoin $RPM_BUILD_ROOT/etc/ssl/rng $RPM_BUILD_ROOT/var/lib/rng/.rngcoin $RPM_BUILD_ROOT/usr/lib/systemd/system $RPM_BUILD_ROOT/etc/logrotate.d
%{__install} -m 755 src/rngcoind $RPM_BUILD_ROOT%{_bindir}
%{__install} -m 755 src/rngcoin-cli $RPM_BUILD_ROOT%{_bindir}
%{__install} -m 600 contrib/redhat/rngcoin.conf $RPM_BUILD_ROOT/var/lib/rng/.rngcoin
%{__install} -m 644 contrib/redhat/rngcoind.service $RPM_BUILD_ROOT/usr/lib/systemd/system
%{__install} -m 644 contrib/redhat/rngcoind.logrotate $RPM_BUILD_ROOT/etc/logrotate.d/rngcoind
%{__mv} -f contrib/redhat/rng $RPM_BUILD_ROOT%{_bindir}

%clean
%{__rm} -rf $RPM_BUILD_ROOT

%pretrans
getent passwd rng >/dev/null && { [ -f /usr/bin/rngcoind ] || { echo "Looks like user 'rng' already exists and have to be deleted before continue."; exit 1; }; } || useradd -r -M -d /var/lib/rng -s /bin/false rng

%post
[ $1 == 1 ] && {
  sed -i -e "s/\(^rpcpassword=MySuperPassword\)\(.*\)/rpcpassword=$(cat /dev/urandom | tr -dc 'a-zA-Z0-9' | fold -w 32 | head -n 1)/" /var/lib/rng/.rngcoin/rngcoin.conf
  openssl req -nodes -x509 -newkey rsa:4096 -keyout /etc/ssl/rng/rngcoin.key -out /etc/ssl/rng/rngcoin.crt -days 3560 -subj /C=US/ST=Oregon/L=Portland/O=IT/CN=rngcoin.rng
  ln -sf /var/lib/rng/.rngcoin/rngcoin.conf /etc/rngcoin/rngcoin.conf
  ln -sf /etc/ssl/rng /etc/rngcoin/certs
  chown rng.rng /etc/ssl/rng/rngcoin.key /etc/ssl/rng/rngcoin.crt
  chmod 600 /etc/ssl/rng/rngcoin.key
} || exit 0

%posttrans
[ -f /var/lib/rng/.rngcoin/addr.dat ] && { cd /var/lib/rng/.rngcoin && rm -rf database addr.dat nameindex* blk* *.log .lock; }
sed -i -e 's|rpcallowip=\*|rpcallowip=0.0.0.0/0|' /var/lib/rng/.rngcoin/rngcoin.conf
systemctl daemon-reload
systemctl status rngcoind >/dev/null && systemctl restart rngcoind || exit 0

%preun
[ $1 == 0 ] && {
  systemctl is-enabled rngcoind >/dev/null && systemctl disable rngcoind >/dev/null || true
  systemctl status rngcoind >/dev/null && systemctl stop rngcoind >/dev/null || true
  pkill -9 -u rng > /dev/null 2>&1
  getent passwd rng >/dev/null && userdel rng >/dev/null 2>&1 || true
  rm -f /etc/ssl/rng/rngcoin.key /etc/ssl/rng/rngcoin.crt /etc/rngcoin/rngcoin.conf /etc/rngcoin/certs
} || exit 0

%files
%doc COPYING
%attr(750,rng,rng) %dir /etc/rngcoin
%attr(750,rng,rng) %dir /etc/ssl/rng
%attr(700,rng,rng) %dir /var/lib/rng
%attr(700,rng,rng) %dir /var/lib/rng/.rngcoin
%attr(600,rng,rng) %config(noreplace) /var/lib/rng/.rngcoin/rngcoin.conf
%attr(4750,rng,rng) %{_bindir}/rngcoin-cli
%defattr(-,root,root)
%config(noreplace) /etc/logrotate.d/rngcoind
%{_bindir}/rngcoind
%{_bindir}/rng
/usr/lib/systemd/system/rngcoind.service

%changelog
* Thu Aug 31 2017 Aspanta Limited <info@aspanta.com> 0.6.3
- There is no changelog available. Please refer to the CHANGELOG file or visit the website.
