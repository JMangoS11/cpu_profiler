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
    virsh vcpupin e-vm3 $i $i
done





# Start the prober
output_title="7prober_output_$(date +%d%H%M).txt"
# Start the prober
ssh -T ubuntu@e-vm1 "output_file=$output_title; echo \"\$(date): Starting prober\" >> \"\$output_file\";nohup sudo ./a.out -p 100 -d 3 -s 1000 -v -i 20 >> \"\$output_file\" 2>&1 &"



# Wait a minute to let the prober measure
sleep 60


ssh -T ubuntu@e-vm1  << EOF
    echo "$(date): First Minute of Measurement Finished,One Third Competition Intialized"  >> ${output_title}
EOF

ssh -T ubuntu@e-vm3 "nohup sudo ./a.out -p 30 -s 60 -i 10000000 &" &

sleep 60
ssh -T ubuntu@e-vm1  << EOF
    echo "$(date): Second Minute of Measurement Fished, Half Competition Initialized" >> ${output_title}
EOF

ssh -T ubuntu@e-vm3 "sudo killall a.out"
ssh -T ubuntu@e-vm3 "nohup sudo ./a.out -p 45 -s 45 -i 1000000 &" &
sleep 60

ssh -T ubuntu@e-vm1  << EOF
    echo "$(date): Third Minute of Measurement Finished, Two Thirds Competition Initialized" >> ${output_title}
EOF

ssh -T ubuntu@e-vm3 "sudo killall a.out"
ssh -T ubuntu@e-vm3 "nohup sudo ./a.out -p 60 -s 30 -i 1000000 &" &

sleep 60
