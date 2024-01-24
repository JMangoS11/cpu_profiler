sudo bash /home/ubuntu/cpu_profiler/create_cgroups.sh
sudo insmod /home/ubuntu/vsched/custom_modules/preempt_proc.ko
sudo insmod /home/ubuntu/vsched/custom_modules/cust_capacity.ko
sudo insmod /home/ubuntu/vsched/custom_modules/max_latency.ko



