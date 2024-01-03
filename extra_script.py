Import("env")
import datetime

#Change the name of the output firmware file to include relevant information
date = datetime.datetime.now().strftime("%Y%m%d")
env.Replace(PROGNAME=f"{env.GetProjectOption('project_name')}_{env.GetProjectOption('environment_name')}_{date}")
