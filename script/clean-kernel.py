import sys
import commands

class executeException(BaseException):
    pass

def execute(cmd):
    status, output = commands.getstatusoutput(cmd)
    if status != 0:
        raise executeException
    return output

if __name__ == "__main__":
    origin_dir = "/usr/src/linux-3.13.bak"
    code_dir = "/usr/src/linux-3.13"
    output = execute("diff -cabr %s %s | grep diff | awk '{print $3,$4}'" % (origin_dir, code_dir))
    items = output.split('\n')
    for item in items:
        print "find diff: %s" % item
        print "handle it..."
        path = item.split(' ')
        execute("cp %s %s" % (path[0], path[1]))


