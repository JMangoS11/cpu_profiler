sudo sh /home/jmangos/cpu_profiler/create_cgroups.sh
sudo rmmod preempt_proc
sudo rmmod cust_capacity
sudo rmmod max_latency
sudo rmmod cust_topo
sudo insmod /home/jmangos/vsched/custom_modules/preempt_proc.ko
sudo insmod /home/jmangos/vsched/custom_modules/cust_capacity.ko
sudo insmod /home/jmangos/vsched/custom_modules/max_latency.ko
sudo insmod /home/jmangos/vsched/custom_modules/cust_topo.ko


