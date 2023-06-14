if ! grep -q "cpu" /sys/fs/cgroup/cgroup.subtree_control; then 
    sudo echo "+cpu" >> /sys/fs/cgroup/cgroup.subtree_control; 
fi 

if [ ! -d /sys/fs/cgroup/test_prgroup ]; then
    sudo mkdir /sys/fs/cgroup/test_prgroup
fi



sudo echo "threaded" > /sys/fs/cgroup/test_prgroup/cgroup.type; 
sudo echo 1 > /sys/fs/cgroup/lw_prgroup/cpu.idle
sudo echo -20 > /sys/fs/cgroup/test_prgroup/cpu.weight.nice
