import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
from sklearn.model_selection import train_test_split
from sktime.classification.distance_based import KNeighborsTimeSeriesClassifier
from sklearn.metrics import confusion_matrix
from sklearn.metrics import accuracy_score
from sklearn.metrics import classification_report
from sklearn.metrics import ConfusionMatrixDisplay
import tikzplotlib

from os import listdir
from os.path import isfile, join

path = "../../results/website_fp/"
filt = ["google", "youtube", "tmall", "facebook", "qq", "baidu", "sohu", "taobao", "360", "jd", "amazon", "yahoo", "wikipedia", "zoom", "netflix"]
do_filt = False
maxtraces = -1


files_all = [ f for f in listdir(path) if isfile(join(path, f)) and ".txt" in f ]
files = []
if do_filt:
    for f in files_all:
        ok = False
        for fi in filt:
            if fi in f:
                ok = True
                break
        if ok:
            if maxtraces != -1:
                if int(f.split("_")[1].split(".")[0]) >= maxtraces:
                    ok = False

        if ok:
            files.append(f)
else: files = files_all

print(files)

sequences = list()
target = list()

maxlen = 0
for f in files:
    if do_filt and not ok: continue
    df = pd.read_csv(path + f, header=0)
    values = df.values.flatten()
    maxlen = max(len(values), maxlen)
print("Trace length: %d" % maxlen)
maxlen = 230

for f in files:
    df = pd.read_csv(path + f, header=0)
    values = df.values.flatten()
    if len(values) > maxlen:
        values = values[:maxlen]
    np_values = np.array(values, dtype=np.double)
    np_values = np.pad(np_values, (0, maxlen - len(values)), 'constant')
    sequences.append(np_values)
    target.append(f.split("_")[0])

X = pd.DataFrame({'col1': sequences })
y = pd.Series(target)

X_train, X_test, y_train, y_test = train_test_split(X.iloc[:], y)
print(X_train.shape, y_train.shape, X_test.shape, y_test.shape)

labels, counts = np.unique(y_train, return_counts=True)


knn = KNeighborsTimeSeriesClassifier(n_neighbors=1, distance="dtw", n_jobs=-1)
knn.fit(X_train, y_train)
print("KNN score:")
print(knn.score(X_test, y_test))

y_pred = knn.predict(X_test)
print(y_pred)
print(accuracy_score(y_test, y_pred))
print(classification_report(y_test, y_pred))
cm = confusion_matrix(y_test, y_pred)
print(cm)
for line in cm:
    lsum = max(1, sum(line.tolist()))
    print(",".join([str(100.0 * x / lsum) for x in line.tolist()]))

ConfusionMatrixDisplay.from_predictions(y_test, y_pred)
plt.xticks(rotation=45)

tikzplotlib.save("confusion.tikz")
plt.show()
