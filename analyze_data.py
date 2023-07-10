import pandas as pd
from collections import defaultdict

# Open and read the file
with open('7prober_output_101546.txt', 'r') as f:
    lines = f.readlines()

# Initialize a dictionary to store the parsed data
parsed_data = defaultdict(lambda: defaultdict(list))

current_cpu = None

for line in lines:
    # Identify CPU lines and parse the CPU
    if line.startswith('CPU:'):
        current_cpu = line.split(" ")[0].split(":")[1]
    # Ignore lines starting with 'Fri' or '---', or too short lines
    elif line.startswith('Mon') or len(line)<3 or line.startswith('---')or line.startswith('  '):
        print('start')
    # For other lines, parse values and add to the dictionary
    else:
        # remove any leading colon from line
        if(line[0]==":"):
            line = line[1:]
        print(line)
        values = line.split(':') 
        if(len(values)<2):
            break
        for i in range(0, len(values), 2):
            # Get the label (e.g., 'Capacity Perc') and the value
            label, value = values[i], float(values[i+1])
            
            # Append the value to the appropriate list
            parsed_data[label][current_cpu].append(value)

# Convert the dictionary to a DataFrame
df = pd.DataFrame({key: pd.Series(val) for key, val in parsed_data.items()})

print(df)

import matplotlib.pyplot as plt

# For each label...
for label, cpu_dict in parsed_data.items():
    # Create a new figure
    plt.figure(figsize=(10, 6))
    # For each CPU...
    for cpu, values in cpu_dict.items():
        # Plot the data
        print(values)
        plt.plot(values, label=cpu)
    # Add a legend, title, and labels
    plt.legend()
    plt.title(label)
    plt.xlabel('Time step')
    plt.ylabel('Value')
    # Show the plot
    plt.show()