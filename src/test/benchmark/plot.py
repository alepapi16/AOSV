import numpy as np
import matplotlib.pyplot as plt
import seaborn as sns
import bisect

d = {}
loads = []
lens = []
with open("data/rw_tps.data") as f:
	for line in f:
		v = line.split()
		# parse load
		load = int(v[0].split("=")[1])
		if not load in loads:
			bisect.insort(loads, load)
		# parse load
		len = int(v[1].split("=")[1])
		if not len in lens:
			bisect.insort(lens, len)
		# parse tps
		tps = float(v[3].split("=")[1])
		
		if not load in d:
			d[load] = {}
		if not len in d[load]:
			d[load][len] = {}
			d[load][len]["tps"] = tps/10**3
			d[load][len]["count"] = 1
		else:
			d[load][len]["tps"] += tps/10**3
			d[load][len]["count"] += 1
			

m = [[d[i][j]["tps"]/d[i][j]["count"] for i in loads] for j in lens]

ax = sns.heatmap(m, annot_kws={"size":11}, xticklabels=loads, yticklabels=lens, annot=True, linewidths=1, cmap="flare", fmt='3.0f', cbar_kws={'label': 'Transactions per second', 'format': '%d K'})  #annot_kws={"size":15}
for t in ax.texts: t.set_text(t.get_text() + " K")

plt.xlabel("Load (#threads)")
plt.ylabel("Message size (B)")
plt.gca().invert_yaxis()
plt.savefig('results/rw_tps.png', bbox_inches='tight')
