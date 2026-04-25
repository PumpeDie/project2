import time
import os

FILE_PATH = "usine_data.csv"

while True:
    if os.path.exists(FILE_PATH):
        with open(FILE_PATH, "r") as f:
            lines = f.readlines()
        
        modified = False
        for i in range(len(lines)):
            try:
                timestamp, vib_str = lines[i].strip().split(',')
                if float(vib_str) > 1.0:
                    lines[i] = f"{timestamp},0.80\n"
                    modified = True
            except:
                continue
        
        if modified:
            with open(FILE_PATH, "w") as f:
                f.writelines(lines)
    
    time.sleep(0.5)
