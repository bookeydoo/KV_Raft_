import subprocess
import time 


def read_Conf_File(filename):
    array=[]
    ip   = None
    port = None
    
    with open(filename,"r") as f:    
        for line in f:
            if "Ip=" in line and "port=" in line :
                #split at ip
                part=line.split("Ip=")[1]
                #first quoted string is ip
                ip=part.split('"')[1]
                
                part=line.split("port=")[1]
                port=part.split('"')[1]

                array.append(port)
                
    return array
                
        
ConfigFile="Config.txt"
array =read_Conf_File(ConfigFile)

print("Starting all nodes in Config file")
processArray=[]


try:
    #Start the processes
    for element in array:
        process=subprocess.Popen(["raft_server.exe","-F",element])
        processArray.append(process)

    #graceful shutdown
    time.sleep(30)
    
except KeyboardInterrupt:
    print("\nInterrupted!\nKilling processes...")



finally:
    for proc in processArray:
        if proc.poll() == None:
            try: #graceful shutdown
                proc.terminate()
                proc.wait(timeout=5)
                
            except subprocess.TimeoutExpired:
                #if graceful shutdown didnt work 
                proc.kill()
                proc.wait()
                

print("\nAll processes shut down")
                    
        