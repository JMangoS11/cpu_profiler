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
output_title="6prober_output_$(date +%d%H%M).txt"
# Start the prober
ssh -T ubuntu@e-vm1 "output_file=$output_title; echo \"\$(date): Beginning test 6:Dynamic InVCPU testing\" >> \"\$output_file\";nohup sudo ./a.out -p 100 -s 2000 -v -i 20 -d 2 >> \"\$output_file\" 2>&1 &"

ssh -T ubuntu@e-vm1 "echo '$(date): Initialize competition and sysbench' >> ${output_title}"
ssh -T ubuntu@e-vm2 'nohup sysbench --threads=16 --time=900000 cpu run &' &


# Wait a minute to let the prober measure
sleep 60


ssh -T ubuntu@e-vm1  << EOF
    echo "$(date): First Minute of Measurement Finished, generating workload at 20%" >> ${output_title}
EOF

ssh -T ubuntu@e-vm1 "nohup sudo ./work.out -p 100 -s 400 -i 50 &" &

sleep 60

ssh -T ubuntu@e-vm1  << EOF
    echo "$(date): Second Minute of Measurement Finished, generating workload at 40%" >> ${output_title}
EOF

ssh -T ubuntu@e-vm1 "sudo killall work.out" &
ssh -T ubuntu@e-vm1 "nohup sudo ./work.out -p 160 -s 240  -i 50 &" &
sleep 60

ssh -T ubuntu@e-vm1  << EOF
    echo "$(date): Third Minute of Measurement Finished, generating workload at 80%" >> ${output_title}
EOF


ssh -T ubuntu@e-vm1 "sudo killall work.out" &
ssh -T ubuntu@e-vm1 "nohup sudo ./work.out -p 320 -s 80 -i 50 &" &


sleep 60


ssh -T ubuntu@e-vm1  << EOF
    echo "$(date): Fourth Minute of Measurement Finished, generating workload at 100%" >> ${output_title}
EOF


ssh -T ubuntu@e-vm1 "sudo killall work.out" &
ssh -T ubuntu@e-vm1 'nohup sysbench --threads=16 --time=900000 cpu run &' &


sleep 60