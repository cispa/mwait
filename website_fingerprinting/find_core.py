import sys
import requests
import time

def get_irqs():
    irq_info = {}
    all_irq = open("/proc/interrupts").read().strip().split("\n")
    for irq in all_irq:
        data = irq.split(":")
        if len(data) == 2:
            irq_info[data[0].strip()] = data[1].strip().split()
    return irq_info

def irq_diff(irq1, irq2):
    irq_diff = {}
    for i in irq1:
        irq_diff[i] = irq1[i]
        for d in range(len(irq1[i])):
            try:
                irq_diff[i][d] = abs(int(irq2[i][d]) - int(irq1[i][d]))
            except:
                pass
    return irq_diff

def irq_print(irq):
    for i in irq:
        print("%3s: " % i, end='')
        for v in irq[i]:
            print("%10s " % str(v), end='')
        print("")
        
def irq_clean(irq):
    cleaned = {}
    for i in irq:
        try:
            int(i)
        except:
            continue
        cleaned[i] = irq[i]
        for v in range(len(irq[i])):
            try:
                cleaned[i][v] = int(irq[i][v])
            except:
                cleaned[i][v] = 0
    return cleaned

start = get_irqs()
for i in range(10):
    r = requests.get('https://www.youtube.com')
end = get_irqs()

network = irq_diff(start, end)
#irq_print(network)
clean = irq_clean(network)
#irq_print(clean)

cores = []
values = []
for i in clean:
    if sum(clean[i]) == 0:
        continue
    mv = max(clean[i])
    cores.append(clean[i].index(mv))
    values.append(mv)
maxval = max(values)
idx = values.index(maxval)

print("Core %d" % cores[idx])

o = open("core_observer.h", "w")
o.write("#define CORE_OBSERVER %d\n" % cores[idx])
o.close()

