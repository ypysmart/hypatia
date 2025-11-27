import re
# Script to extract lines containing specific phrases from a text file and save to a new file

# Define the input and output file paths
input_file = 'cwnd-console5.txt'  # Replace with your actual input file path
output_file = 'extracted_lines.txt'  # The new file where extracted lines will be saved

# Define the phrases to search for
phrases = [
    "In recovery，，tcbd->m_cWnd:",
    "In slow start，，tcbd->m_cWnd:",
    "In Congestion Avoidance tcbd->m_cWnd:"
]

# Read the input file and collect matching lines
extracted = []
with open(input_file, 'r', encoding='utf-8') as infile:
    for line in infile:
        # Check if any phrase is in the line
        if any(phrase in line for phrase in phrases):
            extracted.append(line)

# Write the extracted lines to the output file
with open(output_file, 'w', encoding='utf-8') as outfile:
    outfile.writelines(extracted)

print(f"Extracted {len(extracted)} lines to {output_file}")