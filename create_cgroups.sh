sudo su
if ! grep -q "cpu" /sys/fs/cgroup/cgroup.subtree_control; then 
    echo "+cpu" >> /sys/fs/cgroup/cgroup.subtree_control; 
fi 
if [ ! -d /sys/fs/cgroup/lw_prgroup ]; then
    mkdir /sys/fs/cgroup/lw_prgroup
fi
if [ ! -d /sys/fs/cgroup/hi_prgroup ]; then
    mkdir /sys/fs/cgroup/hi_prgroup
fi
echo 1 > /sys/fs/cgroup/lw_prgroup/cpu.idle
echo -20 > /sys/fs/cgroup/hi_prgroup/cpu.weight.nice
