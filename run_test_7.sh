#!/bin/bash
source ~/.bashrc
source ~/.bash_profile

sudo echo 3000000 > /sys/kernel/debug/sched/min_granularity_ns
# Start a VM with 16 cores, treat the cores as four groups of four cores
for i in {0..15}
do
    virsh vcpupin e-vm1 $i $i
done

for i in {0..15}
do
    virsh vcpupin e-vm2 $i $i
done





# Start the prober
output_title="/cpu_profiler/test_logs/7prober_output_$(date +%d%H%M).txt"
# Start the prober
ssh -T ubuntu@e-vm1 "output_file=$output_title; echo \"\$(date): Beginning test 6:Dynamic OutVCPU testing\" >> \"\$output_file\";nohup sudo ./a.out -p 100 -s 1000 -v -i 20 -d 1 >> \"\$output_file\" 2>&1 &"

ssh -T ubuntu@e-vm1 "echo '$(date): Initialize competition and sysbench' >> ${output_title}"

# Wait a minute to let the prober measure
sleep 10


ssh -T ubuntu@e-vm1  << EOF
    echo "$(date): First Minute of Measurement Finished,One Third Competition Intialized  >> ${output_title}
EOF

ssh -T ubuntu@e-vm2 "nohup sudo ./a.out -p 50 -s 100 -i 1 &"

sleep 60
ssh -T ubuntu@e-vm1  << EOF
    echo "$(date): Second Minute of Measurement Fished, Half Competition Initialized" >> ${output_title}
EOF

ssh -T ubuntu@e-vm2 "sudo killall a.out"
ssh -T ubuntu@e-vm2 "nohup sudo ./a.out -p 75 -s 75 -i 1 &"
sleep 60

ssh -T ubuntu@e-vm1  << EOF
    echo "$(date): Third Minute of Measurement Finished, Two Thirds Competition Initialized" >> ${output_title}
EOF

ssh -T ubuntu@e-vm1 "sudo killall a.out"
ssh -T ubuntu@e-vm1 "nohup sudo ./a.out -p 100 -s 25 -i 1 &"

sleep 60
