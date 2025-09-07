import subprocess

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
for element in array:
    subprocess.run(["raft_server.exe",element])
    