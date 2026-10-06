#!/usr/bin/env python3
"""Plot the recorded C++ benchmark summaries using Python Matplotlib."""
from pathlib import Path
import argparse
import csv
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np

ROOT=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('summary',type=Path,nargs='?',default=ROOT/'results/bench_20261007_034644/summary.csv')
parser.add_argument('--output',type=Path,default=ROOT/'docs/figures/pipeline_times.png')
args=parser.parse_args()
with args.summary.open(newline='') as f:rows=list(csv.DictReader(f))
lookup={(r['workload'],int(r['windows']),r['mode']):r for r in rows}
sizes=sorted({int(r['windows']) for r in rows})
modes=[('sequential','Sequential','#555555'),('mpi','MPI','#2274A5'),('hybrid-static','Hybrid static','#2D936C'),('hybrid-dynamic','Hybrid dynamic','#A63D40')]
plt.rcParams.update({'font.size':12,'axes.spines.top':False,'axes.spines.right':False})
fig,axes=plt.subplots(2,1,figsize=(9,7.5))
for ax,workload in zip(axes,['uniform','skewed']):
    maximum=max(float(lookup[(workload,n,m)]['max_seconds']) for n in sizes for m,_,_ in modes)
    for index,(mode,label,color) in enumerate(modes):
        rs=[lookup[(workload,n,mode)] for n in sizes]
        med=[float(r['median_seconds']) for r in rs]
        low=[v-float(r['min_seconds']) for v,r in zip(med,rs)]
        high=[float(r['max_seconds'])-v for v,r in zip(med,rs)]
        x=np.arange(len(sizes))+(index-1.5)*.19
        ax.bar(x,med,width=.175,color=color,label=label,yerr=[low,high],capsize=3,error_kw={'elinewidth':1,'capthick':1,'ecolor':'#222222'},zorder=3)
        for px,r,v in zip(x,rs,med):
            text=f'{v:.3f}' if v<.1 else f'{v:.2f}'
            ax.text(px,float(r['max_seconds'])+maximum*.025,text,ha='center',va='bottom',fontsize=10)
    ax.set_xticks(range(len(sizes)),[f'{n:,}' for n in sizes])
    ax.set_ylim(0,maximum*1.22)
    ax.set_ylabel('Pipeline time (seconds)')
    ax.set_xlabel('Sensor windows')
    ax.set_title('Uniform workload' if workload=='uniform' else 'Uneven workload',loc='left',fontweight='bold')
    ax.grid(axis='y',alpha=.2,zorder=0)
fig.legend(*axes[0].get_legend_handles_labels(),loc='upper center',ncol=4,frameon=False,bbox_to_anchor=(.5,1.005),fontsize=11)
fig.tight_layout(rect=(0,0,1,.95),h_pad=2)
args.output.parent.mkdir(parents=True,exist_ok=True)
fig.savefig(args.output,dpi=220,bbox_inches='tight')
plt.close(fig)
print(f'Plotted {len(rows)} summary groups from {args.summary} to {args.output}')
