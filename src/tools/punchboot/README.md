
# Install udev rule to allow punchboot access
sudo cp 30-punchboot.rules /etc/udev/rules.d/
sudo udevadm trigger
