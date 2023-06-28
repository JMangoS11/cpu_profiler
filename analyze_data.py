import pandas as pd
f = open('prober_output.txt', 'r')
data = f.read()
lines = data.strip().split('\n')

# Initialize an empty list to store the parsed data
parsed_data = []

current_cpu = None

for line in lines:
    if line.startswith('CPU:'):
        current_cpu = line.split()[0]
    elif line.startswith('Wed'):
        print('start')
    else:
        print(line)
        values = line.split(':')
        print(values)

# Create a pandas DataFrame from the parsed data
df = pd.DataFrame(parsed_data, columns=['CPU', 'Label', 'Value'])

# Pivot the table to have 'Label' as columns and 'CPU' as index
df_pivot = df.pivot(index='CPU', columns='Label', values='Value')

# Reorder the columns if necessary
desired_columns = [
    'Capacity Perc', 'Latency', 'Preempts', 'Capacity Raw',
    'Cperc stddev', 'Cperc ema', 'Latency EMA'
]
df_pivot = df_pivot[desired_columns]

# Print the DataFrame
print(df_pivot)