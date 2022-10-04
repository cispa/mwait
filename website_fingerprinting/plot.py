import matplotlib.pyplot as plt
import sys

legends = []
for p in sys.argv[1:]:
    d = [ int(x) for x in open(p).read().strip().split("\n") ]
    plt.plot(d)
    legends.append(p)
plt.legend(legends)
plt.show()

