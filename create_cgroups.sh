if ! grep -q "cpu" /sys/fs/cgroup/cgroup.subtree_control; then 
    sudo echo "+cpu" >> /sys/fs/cgroup/cgroup.subtree_control; 
fi 
if [ ! -d /sys/fs/cgroup/lw_prgroup ]; then
    sudo mkdir /sys/fs/cgroup/lw_prgroup
fi
if [ ! -d /sys/fs/cgroup/hi_prgroup ]; then
    sudo mkdir /sys/fs/cgroup/hi_prgroup
fi
sudo echo 1 > /sys/fs/cgroup/lw_prgroup/cpu.idle
sudo echo -20 > /sys/fs/cgroup/hi_prgroup/cpu.weight.nice
