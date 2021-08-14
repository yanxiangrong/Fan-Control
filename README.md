# 适用于香橙派的CPU风扇控制程序

systemctl start fan-control
systemctl enable fan-control
systemctl unmask fan-control
dpkg -i fanControl_1.0-2_all.deb
dpkg -r fan-control

chmod 755 package/usr/bin/fanControl
chmod 755 package/DEBIAN/post*
dpkg -b package fanControl_1.0-2_all.deb
dpkg -r fan-control
dpkg -i fanControl_1.0-2_all.deb
stress -c 4