source("duck.R")
source("commons_iobl.R")

folder =  "finallat10"
pref = "finallat10"
devices=c("i0", 'i1', "k5", "m0", "s0", "wd", "h0",  "mp0")
i1 = loadIobFilesMatching(folder, 'stats-id0', devices)
i2 = loadIobFilesMatching("i4i-lat", 'stats-id0', "i4i")
i2$device = 'i4i'
i2$cmin=0;i2$c10p=0;i2$c20p=0;i2$c25p=0;i2$c30p=0;i2$c40p=0;i2$c50p=0;i2$c60p=0;i2$c70p=0;i2$c75p=0;i2$c80p=0;i2$c85p=0;i2$c90p=0;i2$c92p5=0;i2$c95p=0;i2$c97p5=0;i2$c99p=0;i2$c99p5=0;i2$c99p9=0;i2$c99p99=0;i2$c99p999=0;i2$cmax=0;
i2$ravg=i2$r50p;i2$wavg=i2$w50p;i2$savg=i2$s50p;i2$cavg=i2$c50p;
i2$rcnt=0;i2$wcnt=0;i2$scnt=0;i2$ccnt=0;
i2$rtot=0;i2$wtot=0;i2$stot=0;i2$ctot=0;
i2$r=0;i2$w=0;i2$s=0;i2$c=0;
i2$threadStatsInterval=0
#i3 = loadIobFilesMatching(folder, "stats-id0", "s1-milan")
#i3$device = 'pm5'
stats = rbind(i1, i2)
i1 = loadIobFilesMatching(folder, 'log', devices)
i2 = loadIobFilesMatching("i4i-lat", 'log', "i4i")
i2$device = 'i4i'
#i3 = loadIobFilesMatching(folder, 'log', "s1-milan")
#i3$device = 'pm5'
logs = rbind(i1, i2)
# join
logs = doLogTransformations(logs)
logsm = iobMelt(logs)
logsma = iobAvgOverTime(logsm, "median(", ")")
logsmau = duck("pivot logsma on variable using first(value)")
stats = doStatsTransformations(stats)
statsm = iobLogStatsMelt(stats)
aggcols = paste(setdiff(colnames(statsm), c('time','timediff','value','writemibs','readmibs','writes','reads','fdatasyncs')), collapse=',')
aggcols
statsma = duck("select "%+%aggcols%+%", median(value) as value
				from statsm
				group by "%+%aggcols%+%"") 
head(duck("select * from statsma where device like 'pm5'"), 10)
iobl = duck("select * from logsmau join statsma using (hash,device,filesizegib)")
colnames(iobl) <- make.unique(tolower(colnames(iobl)), sep = "_s")
# percentiles
ioblmf = sqldf("select *, case when variable like 'r%' then 'read' when variable like 'w%' then 'write' end as percentiletype
				from iobl
				where variable not like 's%' 
					and (((variable like 'r%' or variable like 'w%') and variable not like 'rate%' and variable not like '%read%' and variable not like 'Rnd%' ))
					and pattern not like 'sequential' 
					")
ioblmf$percentile = gsub('r', '', ioblmf$variable)
ioblmf$percentile = gsub('w', '', ioblmf$percentile)
ioblmf$percentile = gsub('p$', '', ioblmf$percentile)
ioblmf$percentile = gsub('p', '.', ioblmf$percentile)
ioblmf$percentile = gsub('min', '0', ioblmf$percentile)
ioblmf$percentile = gsub('max', '100', ioblmf$percentile)
ioblmf$percentile = ioblmf$percentile
unique(ioblmf$percentile)
write.csv(ioblmf, "paper-latency.csv")
ioblmf = read.csv("paper-latency.csv")
# TODO: fix this
ioblmf = duck("select * from ioblmf where (devicecol <> 'm' or rate < 70000) and (devicecol <> 'i' or rate < 200000) and MOD(rate, 1000) == 0")

# PAPER: READ
plotLatency = function(firsttype, secondtype, maxReadLat, maxWriteLat, zoomLatMin, zoomLatMax,  bottomMarginLeft) {
ioblmfp = sqldf("select * 
				from ioblmf 
			where rate > 0 and percentile in (99,99.999, 'avg') 
				and (device not in ('pm5','/blk/k5','/blk/mp0', '/blk/intel1'))")
#print(duck("select * from ioblmfp where "))
ioblmfp$percentile[ioblmfp$percentile == '99'] = 'P99'
ioblmfp$percentile[ioblmfp$percentile == '99.999'] = 'P99.999'
ioblmfpf = duck("select * from ioblmfp where  percentiletype == '"%+%tolower(firsttype)%+%"'")
gdevr = ggplot(ioblmfpf, aes(as.numeric(writeiops)/ rndwriteiops, as.numeric(value), shape=percentile, color=percentile)) +
	   annotate("rect", xmin=0, xmax=1.1, ymin=zoomLatMin, ymax=zoomLatMax, fill='gray86') +
	   geom_line() +
	   geom_point(size=1) +
	   #geom_jitter(width=0.03) +
	   geom_vline(xintercept=1, linetype='dashed', color='gray60') +
	   coord_cartesian(ylim=c(0, maxReadLat), xlim=c(0,1.1)) +
	   scale_x_continuous("Write speed [%]", breaks=seq(0, 1, 0.5), labels = scales::percent, expand=c(0,0) ) +
	   scale_y_continuous(breaks=seq(0, maxReadLat, 2500)) +
	   #scale_y_log10("latency [us]") +
	   facet_grid(~factor(device, labels=c('h','s','w','i','m','i4i'), levels=c("/blk/h0","/blk/s0","/blk/wd","/blk/intel0","/blk/m0", 'i4i'))) +
	   #guides(color = guide_legend("percentile"), shape = guide_legend("percentile")) +
	   theme_bw() +
	   #theme(panel.grid.minor = element_blank()) +
	   theme(axis.title.y = element_blank()) +
	   theme(axis.title.x = element_blank(), axis.text.x=element_blank(), axis.ticks.x=element_blank()) +
	   theme(legend.position = "top", legend.margin=margin(t = 0, b=-0.3, unit='cm'), legend.title=element_blank()) + 
	   theme(strip.background.y = element_blank(), strip.text.y = element_blank()) +
       theme(plot.margin=margin(b=1, l=2, r=2))
# PAPER: WRITE
ioblmfpf = duck("select * from ioblmfp where percentiletype == '"%+%tolower(secondtype)%+%"'")
gdevw = ggplot(ioblmfpf, aes(as.numeric(writeiops) / rndwriteiops, as.numeric(value), shape=factor(percentile), color=factor(percentile))) +
	   annotate("rect", xmin=0, xmax=1.1, ymin=0, ymax=maxWriteLat, alpha=0.5, fill='gray90') +
	   geom_line() +
	   geom_point(size=1) +
	   geom_vline(xintercept=1, linetype='dashed', color='gray60') +
	   coord_cartesian(ylim=c(0, maxWriteLat), xlim=c(0, 1.1)) +
	   scale_x_continuous("Write speed [%]", breaks=seq(0, 1, 0.25), labels=c(" 0%", "25%", "50%", "75%", "100% "), expand=c(0,0)) +
	   scale_y_continuous(expand=c(0,0)) +
	   #scale_y_log10("latency [us]") +
	   labs(color="percentile") +
	   #facet_grid(~factor(devicecol, levels=c('h','s','w','i','m','sp','k'))) +
	   facet_grid(~factor(device, labels=c('h','s','w','i','m','i4i'), levels=c("/blk/h0","/blk/s0","/blk/wd","/blk/intel0","/blk/m0", 'i4i'))) +
	   theme_bw() +
	   theme(legend.position = "none", legend.margin=margin(t = -0.1, b=-0.3, unit='cm')) +
	   theme(axis.title.y = element_blank()) +
       theme(strip.background.x = element_blank(), strip.text.x = element_blank()) +
	   theme(strip.background.y = element_blank(), strip.text.y = element_blank()) +
       theme(plot.margin=margin(t=1, l=bottomMarginLeft, r=2))
   #ylabel = textGrob("latency [us]")
   grid.arrange(gdevr, gdevw, ncol=1, nrow=2, heights=c(1, 0.95), left=textGrob(firsttype%+%" latency [μs]", rot=90, gp=gpar(fontsize=15,font=8)))
}
w = 10
h = w/3.2
CairoPDF("paper-latency-write", width=w, height=h, bg = "transparent")
plotLatency("Write", "Write", 5000, 110, -200, 300, 7.2)
dev.off()
CairoPDF("paper-latency-read", width=w, height=h, ,bg = "transparent")
plotLatency("Read", "Read", 5000, 490, -100, 500, 7.2)
dev.off()

