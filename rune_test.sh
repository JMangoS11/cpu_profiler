#!/bin/bash
source ~/.bashrc
source ~/.bash_profile

sudo echo 3000000 > /sys/kernel/debug/sched/min_granularity_ns
# Start a VM with 16 cores, treat the cores as four groups of four cores
for i in {0..15}
do
    virsh vcpupin e-vm1 $i $i
done


for i in {0..11}; do
  pinned_core=$(( (i%4) + 4 ))
  virsh vcpupin e-vm2 $i $pinned_core
done

for i in {12..15}; do
  pinned_core=$(( i - 12 ))
  virsh vcpupin e-vm2 $i $pinned_core
done


ssh -T ubuntu@e-vm1 'sudo rm -rf pre_prober_sysbench.txt;sudo rm -rf post_prober_sysbench.txt'


# Start sysbench on all 16 vcpus for 3 minutes, running in the background
ssh -T ubuntu@e-vm1 << EOF
    sudo nohup sysbench --threads=16 --time=10 cpu run > pre_prober_sysbench.txt &
EOF



# Start the prober
output_title="/cpu_profiler/test_logs/prober_output_$(date +%d%H%M).txt"
# Start the prober
ssh -T ubuntu@e-vm1 "output_file=$output_title; echo \"\$(date): Starting prober\" >> \"\$output_file\";nohup sudo ./a.out -p 100 -s 1000 -v -i 20 >> \"\$output_file\" 2>&1 &"

ssh -T ubuntu@e-vm1 << EOF
    sudo nohup sysbench --threads=16 --time=10 cpu run > post_prober_sysbench.txt &
EOF



ssh -T ubuntu@e-vm1 "echo '$(date): Initialize competition and sysbench' >> ${output_title}"
ssh -T ubuntu@e-vm1 'sudo nohup sysbench --threads=16 --time=900000 cpu run &' &
ssh -T ubuntu@e-vm2 'nohup sysbench --threads=16 --time=900000 cpu run &' &


# Initialize competition on physical cores
#taskset -c 0-3 sysbench --threads=4 --time=90 cpu run &
#taskset -c 4-7 sysbench --threads=1 --time=90 cpu run &
#taskset -c 4-7 sysbench --threads=4 --time=90 cpu run &
#taskset -c 4-7 sysbench --threads=4 --time=90 cpu run &

# Wait a minute to let the prober measure
sleep 10


taskset -c 12-19 sysbench --threads=10 --time=10000 cpu run &

ssh -T ubuntu@e-vm1  << EOF
    echo "$(date): First minute of measurement finished,moving fourth group" >> ${output_title}
EOF

# Move the fourth group to an empty socket
for i in {12..15}
do
    virsh vcpupin e-vm1 $i $((i + 16))
done


# Wait a minute for prober to measure the new configuration
sleep 60

ssh -T ubuntu@e-vm1 << EOF
    echo "$(date): Second minute of measurement finished, after moving the fourth group" >> ${output_title} 
EOF

# Change the min granularity of the host system
sudo echo 6000000 > /sys/kernel/debug/sched/min_granularity_ns

# Wait for another minute to let the prober measure latency changes
sleep 60

ssh -T ubuntu@e-vm1 << EOF
    echo "$(date): Third minute of measurement finished, after changing target latency" >> ${output_title} 
EOF

# Stop the prober
ssh -T ubuntu@e-vm1 << EOF
    sudo killall sysbench
    sudo killall a.out
EOF

ssh -T ubuntu@e-vm2 << EOF
	sudo killall sysbench
EOF

# Collect the output files from the VM
scp ubuntu@e-vm1:/path/to/pre_prober_sysbench.txt /home
scp ubuntu@e-vm1:/path/to/prober_output.txt /home
