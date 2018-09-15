#coding=utf-8

import hashlib

cpcr_list = '''history 8b87f480578e432f0a11f8783b59dbfa593fd8cb
4026532222 6878a26f8b3cf924244012afab916e2db6a2826f d8e8ea4c33f4476778aef29f2b101916c54ed494
4026532238 5c66faea22f4b35d244d666a4b9b62c53228d6bd 37a7f90fdcb6c610d4b19ac579688404b2ffb4ba'''

pcr12 = "7733993702ce721df0d27aad2fa8d5c1d013ebd4"

if __name__ == '__main__':
    history_cpcr, _, cpcrs = cpcr_list.partition('\n')
    _, __, history = history_cpcr.partition(" ")

    print("history: %s" % history)
    _hash = hashlib.sha1()
    _hash.update(bytes.fromhex(history))

    sha1 = hashlib.sha1()

    for cpcr in cpcrs.split('\n'):
        if cpcr == "":
            continue
        fields = cpcr.split(" ")
        value = bytes.fromhex(fields[1])
        secret = bytes.fromhex(fields[2])
        print("cpcr: value=%s secret=%s" % (fields[1], fields[2]))
        
        xor = ""
        for i in range(0, len(value)):
            t = ''.join("%02x" % (value[i] ^ secret[i]))
            xor = xor + t
    
        sha1.update(bytes.fromhex(xor))

    digest = sha1.hexdigest()
    _hash.update(bytes.fromhex(digest))
    pcr = _hash.hexdigest()
    
    if pcr != pcr12:
        print(">>> check failed")
    else:
        print(">>> check success")
    print("[s-pcr, pcr12] = %s %s" % (pcr, pcr12))
