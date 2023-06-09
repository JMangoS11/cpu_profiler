if ! grep -q "cpu" /sys/fs/cgroup/cgroup.subtree_control; then 
    sudo echo "+cpu" >> /sys/fs/cgroup/cgroup.subtree_control; 
fi 



if [ ! -d /sys/fs/cgroup/par_cgroup ]; then
    sudo mkdir /sys/fs/cgroup/par_cgroup
fi

if ! grep -q "threaded" /sys/fs/cgroup/par_cgroup/cgroup.type; then 
    sudo echo "threaded" > /sys/fs/cgroup/par_cgroup/cgroup.type; 
fi 

if ! grep -q "cpu" /sys/fs/cgroup/par_cgroup/cgroup.subtree_control; then 
    sudo echo "+cpu" >> /sys/fs/cgroup/par_cgroup/cgroup.subtree_control; 
fi 

if [ ! -d /sys/fs/cgroup/par_cgroup/hi_prgroup ]; then
    sudo mkdir /sys/fs/cgroup/par_cgroup/hi_prgroup
fi

if [ ! -d /sys/fs/cgroup/par_cgroup/lw_prgroup ]; then
    sudo mkdir /sys/fs/cgroup/par_cgroup/lw_prgroup
fi

sudo echo "threaded" > /sys/fs/cgroup/par_cgroup/lw_prgroup/cgroup.type
sudo echo "threaded" > /sys/fs/cgroup/par_cgroup/hi_prgroup/cgroup.type
sudo echo 1 > /sys/fs/cgroup/par_cgroup/lw_prgroup/cpu.idle
sudo echo -20 > /sys/fs/cgroup/par_cgroup/hi_prgroup/cpu.weight.nice
