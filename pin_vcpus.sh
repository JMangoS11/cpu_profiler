
virsh vcpupin ubuntu_vm_1 0 $(($1))
virsh vcpupin ubuntu_vm_1 1 $(($1+1))
virsh vcpupin ubuntu_vm_1 2 $(($1+2))
virsh vcpupin ubuntu_vm_1 3 $(($1+3))

virsh vcpupin ubuntu_vm_1-clone 0 $(($1))
virsh vcpupin ubuntu_vm_1-clone 1 $(($1+1))
virsh vcpupin ubuntu_vm_1-clone 2 $(($1+2))
virsh vcpupin ubuntu_vm_1-clone 3 $(($1+3))