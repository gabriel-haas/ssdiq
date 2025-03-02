source("duck.R")
source("commons_iobl.R")
library(RColorBrewer)

###################################################################################################
###################################################################################################

# init

# fio
folder = "finalini10"
prefix = "init10-fiozipf-none-with-erase"
i0 = read.csv(folder%+%"/iob-log-"%+%prefix%+%"-san-none-fiozipf.csv")
i0$init = 'none'
folder = "finalinit09"
prefix = "init09-fiozipf"
i3 = read.csv(folder%+%"/iob-log-"%+%prefix%+%"-san-seq-fiozipf.csv")
i3$init = 'seq'
i6 = read.csv(folder%+%"/iob-log-"%+%prefix%+%"-san-seqshuffle-fiozipf.csv")
i6$init = 'seqshuf'
# other
folder = "finalinit07"
prefix = "init07"
i1 = read.csv(folder%+%"/iob-log-"%+%prefix%+%"-san-none-uni.csv")
i1$init = 'none'
i2 = read.csv(folder%+%"/iob-log-"%+%prefix%+%"-san-none-zipf.csv")
i2$init = 'none'
i4 = read.csv(folder%+%"/iob-log-"%+%prefix%+%"-san-seq-uni.csv")
i4$init = 'seq'
i5 = read.csv(folder%+%"/iob-log-"%+%prefix%+%"-san-seq-zipf.csv")
i5$init = 'seq'
i7 = read.csv(folder%+%"/iob-log-"%+%prefix%+%"-san-seqshuffle-uni.csv")
i7$init = 'seqshuf'
i8 = read.csv(folder%+%"/iob-log-"%+%prefix%+%"-san-seqshuffle-zipf.csv")
i8$init = 'seqshuf'
# fio
folder = "finalinit12"
prefix = "init12-noshuffle-zipf"
i9 = read.csv(folder%+%"/iob-log-"%+%prefix%+%"-san-none-zipf.csv")
i9$init = 'none'
i10 = read.csv(folder%+%"/iob-log-"%+%prefix%+%"-san-seq-zipf.csv")
i10$init = 'seq'
iobl = rbind(i0, i1, i2, i3, i4, i5, i6, i7, i8, i9, i10)
# PAPER: Convergence Plot
ioblag = sqldf("select avg(wa) as wa, avg(writesMBs*1024*1024/1e6) as host, 
					avg(nullif(phyWriteMBs*1024*1024/1e6, 0)) as physical,
				  time/250*250 as time, device, pattern, patternDetails, init, bs, hash
			   from iobl
			   where pattern not like 'sequential%' and pattern not like '%seqshuf%'
			   group by time/250, device, pattern, patternDetails, init, bs, hash")
head(ioblag)
ioblagm = melt(ioblag, id = c('time', 'device', 'pattern', 'patternDetails', 'init', 'hash'))
write.csv(ioblagm, "paper-init.csv")
palette_colors=brewer.pal(n = 3, name = "Dark2")
ioblagm = read.csv("paper-init.csv")
ioblagmf = sqldf("select * from ioblagm where (variable == 'host') and pattern in ('uniform', 'zipf-shuffle') and init in ('none','seq')")
ioblagmf$pattern[ioblagmf$pattern == "zipf-shuffle"] = "Zipf"
ioblagmf$pattern[ioblagmf$pattern == "uniform"] = "Uniform"
convergence = ggplot(ioblagmf, aes(x=time/60/60, y=value, color=init)) +
	geom_line(alpha=1) +
	annotate("text", label = "None",  x=1, y=1100, vjust=-0.5, color=palette_colors[1]) + 
	annotate("text", label = "Seq. init", x=1.1, y=200, vjust=0, color=palette_colors[2]) + 
	xlim(0, 4) +
	expand_limits(y=0) +
	xlab("Time [h]") + # TODO change to overwrite times
	ylab("Write [MB/s]") +
	scale_color_manual(values = palette_colors) +
	facet_grid(~ pattern) +
	theme_bw() +
	theme(strip.background.x = element_blank()) +
	theme(legend.position = "none", legend.title=element_blank(), legend.box.margin = margin(b = -10, unit = "pt"))
convergence
widthInline=4
heightInline=2.1
CairoPDF("paper-convergence", width=widthInline, height=heightInline, bg = "transparent")
convergence
dev.off( )

# PAPER: Implied WA plot
ioblag = sqldf("select avg(wa) as wa, avg(writesMBs*1024*1024/1e6) as host, 
					avg(nullif(phyWriteMBs*1024*1024/1e6, 0)) as physical,
				  time/250*250 as time, device, pattern, patternDetails, init, bs, hash
			   from iobl
			   where pattern not like 'sequential%' and pattern not like '%seqshuf%'
			   group by time/250, device, pattern, patternDetails, init, bs, hash")
head(ioblag)
ioblagm = melt(ioblag, id = c('time', 'device', 'pattern', 'patternDetails', 'init', 'hash'))
write.csv(ioblagm, "paper-init.csv")
ioblagm = read.csv("paper-init.csv")
ioblagmf = sqldf("select * from ioblagm where (variable == 'physical' or variable == 'host') and pattern in ('uniform') and init in ('seq')")
palette_colors=brewer.pal(n = 3, name = "Set1")[c(2,1)]
convergence = ggplot(ioblagmf, aes(x=time/60/60, y=value, color=variable, linetype=variable)) +
	geom_line(alpha=1) +
	geom_hline(yintercept=1900, linetype='dashed', color='gray33') +
	xlim(0, 4) +
	ylim(0, 2200) +
	annotate("text", label = "Seq Write spec", x=2.7, y=1900, vjust=-0.5, color="gray33") + 
	#annotate("text", label = "Phy Writes",  x=4, y=1300, vjust=-0.5, color="gray33") + 
	#annotate("text", label = "Host Writes", x=4, y=250,	 vjust=0, color="gray33") + 
	annotate("text", label = "WAF",          x=3.4, y=900, vjust=-0.5, color="gray33") + 
	annotate("segment", x=3, xend=3,  y=550, yend=1650,arrow=arrow(ends='both', length=unit(2,'mm')), size=0.5, color="gray33") +
	annotate("text", label = "Host", x=1, y=220, vjust=0, color=palette_colors[1]) + 
	annotate("text", label = "Physical",  x=1, y=1300, vjust=-0.5, color=palette_colors[2]) + 
	xlab("Time [h]") + # TODO change to overwrite times
	ylab("Write [MB/s]") +
	scale_color_manual(values = palette_colors) +
	theme_bw() +
	theme(legend.position = "none", legend.title=element_blank(), legend.box.margin = margin(b = -10, unit = "pt"))
convergence
widthInline = 3
heightInline = 1.7
CairoPDF("paper-implied-wa", width=widthInline, height=heightInline, bg = "transparent")
convergence
dev.off( )

###################################################################################################
###################################################################################################

# WA throughput <<<<<< this is required for below

folder = "finalwa03c"
prefix = "finalwa03"
i1 = read.csv(folder%+%"/iob-log-h0-"%+%prefix%+%".csv")
i2 = read.csv(folder%+%"/iob-log-i0-"%+%prefix%+%".csv")
i3 = read.csv(folder%+%"/iob-log-k4-"%+%prefix%+%".csv")
#i4 = read.csv(folder%+%"/iob-log-k5-"%+%prefix%+%".csv")
i5 = read.csv(folder%+%"/iob-log-m0-"%+%prefix%+%".csv")
i6 = read.csv(folder%+%"/iob-log-mp0-"%+%prefix%+%".csv")
i7 = read.csv(folder%+%"/iob-log-s0-"%+%prefix%+%".csv")
i8 = read.csv(folder%+%"/iob-log-wd-"%+%prefix%+%".csv")
iobl = rbind(i1, i2, i3, i5, i6, i7, i8)
folder = "finalwa05b"
prefix = "finalwa05"
i1 = read.csv(folder%+%"/iob-log-h0-"%+%prefix%+%".csv")
i2 = read.csv(folder%+%"/iob-log-i0-"%+%prefix%+%".csv")
i3 = read.csv(folder%+%"/iob-log-k4-"%+%prefix%+%".csv")
#i4 = read.csv(folder%+%"/iob-log-k5-"%+%prefix%+%".csv")
i5 = read.csv(folder%+%"/iob-log-m0-"%+%prefix%+%".csv")
i6 = read.csv(folder%+%"/iob-log-mp0-"%+%prefix%+%".csv")
i7 = read.csv(folder%+%"/iob-log-s0-"%+%prefix%+%".csv")
i8 = read.csv(folder%+%"/iob-log-wd-"%+%prefix%+%".csv")
iobl = rbind(iobl, i1, i2, i3, i5, i6, i7, i8)
folder = "finalwa03milan"
prefix = "finalwa03"
im = read.csv(folder%+%"/iob-log-"%+%prefix%+%".csv")
im$device <- str_replace(im$device, ".*s.*", "sp")
iobl = rbind(iobl, im)
folder = "iob-aws-i4i.4xlarge-20240811071208"
im = read.csv(folder%+%"/iob-log-wa-aws-i4i.4xlarge.csv")
im$device <- "i4i"
im$expRate = 1
iobl = rbind(iobl, im)
#iobl = duckdf("select *, case when device like '/blk/i%' then '/blk/i' when device like '/blk/k%' then '/blk/k' when device like 'pm%' then 'pm' else device end as devicecol from iobl")
iobluni2 = duckdf("select *, 'zones' as patterncol from iobl where pattern like 'uniform'")
iobl = duckdf("select *, case when pattern like '%zipf%' then 'zipf' when pattern like '%zones%' then 'zones' when pattern like 'uniform' then 'zipf' else pattern end as patterncol from iobl ")
#duckdf("select distinct(skew) from iobl")
iobl = rbind(iobl, iobluni2)
iobl = duckdf("select *, 
				case  
					when pattern like '%uniform%' and patterncol like 'zones' then 0.5
					when pattern like '%uniform%' and patterncol like 'zipf' then 0
					when pattern like '%sequential%' then -1
					when pattern like '%zipf%' then cast(patterndetails as numeric)
					when patterndetails like '%s0.6%' then 0.6
					when patterndetails like '%s0.7%' then 0.7
					when patterndetails like '%s0.8%' then 0.8
					when patterndetails like '%s0.95%' then 0.95
					when patterndetails like '%s0.9%' then 0.9
					else -2
					end as skew	
				from iobl")
#duckdf("select skew, string_agg(distinct(pattern)), string_agg(distinct(patterncol)) from iobl group by skew")
unique(iobl$device)
iobl$devicecol = iobl$device
iobl$devicecol <- str_replace(iobl$devicecol, ".*/h.*", "h")
iobl$devicecol <- str_replace(iobl$devicecol, ".*/intel.*", "i")
iobl$devicecol <- str_replace(iobl$devicecol, ".*/k.*", "k")
iobl$devicecol <- str_replace(iobl$devicecol, ".*/mp.*", "mw")
iobl$devicecol <- str_replace(iobl$devicecol, ".*/m.*", "m")
iobl$devicecol <- str_replace(iobl$devicecol, ".*/s.*", "s")
iobl$devicecol <- str_replace(iobl$devicecol, ".*/wd.*", "w")
devices = read.csv("devices.csv")
colnames(iobl) <- make.unique(tolower(colnames(iobl)), sep = "_s")
c(colnames(iobl))
#iobl$writesmbs = as.double(iobl$writeiops) * iobl$bs / 1024 / 1024
#head(iobl$writesmbs)
iobl = duckdf("select * from iobl join devices using (devicecol)")
duckdf("select count(distinct(device)) from iobl")
ioblnoavg = duckdf("select *, 
				   avg(writesMBs) OVER (
						partition by device, pattern, patterncol, patterndetails
						order by time ASC ROWS BETWEEN 10 PRECEDING AND 10 FOLLOWING) as winavg
				   from iobl")
head(ioblnoavg)
unique(ioblnoavg$devicecol)
head(duck("select time,device,devicecol from iobl where device like '%i4i%' order by random()"), 2)
head(duck("select time,device,devicecol,winavg from ioblnoavg order by random()"), 2)
# filter noise 
iobl = duckdf("select * from ioblnoavg where time <> 0 and writesMBs > 0.5*winavg and writesMBs < 1.5*winavg") 
unique(iobl$devicecol)
cat('smooth data, removed: ') 
nrow(ioblnoavg) - nrow(iobl)
iobl = duckdf("select *, cast(seqWriteMBs*1e6/1024/1024 as double) / writesMBs as impliedWA, max(writesMBs) over (partition by device) * 1.0 / writesMBs as impliedWA2 from iobl")

# absolute plots
ssds = "('h', 's', 'm', 'w', 'i')"
ioblag = sqldf("select avg(nullif(wa, 0)) as wa, avg(impliedWA) as impliedWA, avg(impliedWA2) as impliedWA2, avg(writesMBs) as writesMBs, avg(readsMBs) as readsMBs, min(time) as time, device, deviceCol, pattern, patterncol, skew, patternDetails, bs, hash
			   from iobl
			   where pattern <> 'sequential' and (devicecol in "%+%ssds%+%" )
			   group by time/1000, device, deviceCol, pattern, patterncol, skew, patternDetails, hash")
write.csv(ioblag, "paper-wa.csv")
ioblag = read.csv("paper-wa.csv")
ioblagm = melt(ioblag, id=c('hash', 'pattern', 'patterncol', 'skew', 'patterndetails', 'device', 'devicecol', 'time'))
ioblagmf = sqldf("select * from ioblagm where variable like 'writesMBs' and patterncol like 'zipf'")
ggplot(ioblagmf, aes(x=time, y=value, color=interaction(skew), shape=variable, linetype=variable)) +
	geom_line(alpha=0.9) +
	#geom_point(alpha=0.5, size=0.75) +
	facet_grid(patterncol ~ device) +
	theme_bw()

width3=3.3
height3=width3*3/4
# PAPER: OVERVIEW: WA vs skew 
wa = sqldf("select device, deviceCol, pattern, patterncol, skew, patternDetails, bs, hash,
		   avg(wa) as wa,
		   avg(writesMBs) avgWritesMBs,
		   avg(impliedWA) as impliedWA,
		   avg(impliedWA2) as impliedWA2,
		   max(time)
	  from ioblag o
	   where 
		time > (select max(time) from ioblag ii where ii.device = o.device and ii.hash = o.hash)*0.6 and
		time < (select max(time) from ioblag ii where ii.device = o.device and ii.hash = o.hash)*0.99
	  group by device, deviceCol, pattern, patterncol, skew, patternDetails, bs, hash")
head(wa)
wa = sqldf("select device, deviceCol, pattern, patterncol, skew, patternDetails, bs, hash,
			ifnull(wa, impliedWA) as wa, case when wa is null then 'implied' else 'measured (OCP)' end as watype
		from wa")
wam = melt(wa, id = c('device', 'devicecol', 'pattern', 'patterncol', 'skew', 'patterndetails', 'bs', 'hash', 'watype'))
wamf = sqldf("select * from wam where skew >= 0")
overviewwa = ggplot(duckdf("select * from wamf where patterncol like 'zones' and devicecol not in ('mw')"), aes(x=factor(skew), y=value, color=devicecol, shape=devicecol, linetype=factor(watype, ordered=TRUE, levels=c("measured (OCP)", "implied")), group=interaction(device, variable))) +
	geom_point() +
	geom_line() +
	expand_limits(y=0) +
	scale_x_discrete(breaks=c(0.5, 0.6, 0.7, 0.8, 0.9, 0.95), labels=c('uniform', '60/40', '70/30', '80/20', '90/10', '95/5')) +
	#scale_y_continuous(expand=expansion(add = c(0,5))) +
	ylab("WAF") +
	xlab("Two-Zones") +
	geom_dl(aes(label=devicecol), method = list(dl.trans(x = x + .2), "last.points")) +
	scale_linetype_manual(values = c('measured (OCP)' = "solid", 'implied' = "dashed")) +
	scale_colour_discrete(guide = FALSE) +
	scale_shape_manual(values=1:9, guide = FALSE) +
	theme_bw() + 
	theme(legend.position=c(0.3,0.2), legend.title=element_blank())
overviewwa
CairoPDF("paper-overview-wa", width=width3, height=height3, bg = "transparent")
overviewwa
dev.off()
# -----: ZIPF: WA vs skew 
wa = sqldf("select device, deviceCol, pattern, patterncol, skew, patternDetails, bs, hash,
		   avg(wa) as wa,
		   avg(writesMBs) avgWritesMBs,
		   avg(impliedWA) as impliedWA,
		   avg(impliedWA2) as impliedWA2,
		   max(time)
	  from ioblag o
	   where 
		time > (select max(time) from ioblag ii where ii.device = o.device and ii.hash = o.hash)*0.6 and
		time < (select max(time) from ioblag ii where ii.device = o.device and ii.hash = o.hash)*0.99
	  group by device, deviceCol, pattern, patterncol, skew, patternDetails, bs, hash")
head(wa)
wa = sqldf("select device, deviceCol, pattern, patterncol, skew, patternDetails, bs, hash,
			ifnull(wa, impliedWA) as wa, case when wa is null then 'implied' else 'measured (OCP)' end as watype
		from wa")
wam = melt(wa, id = c('device', 'devicecol', 'pattern', 'patterncol', 'skew', 'patterndetails', 'bs', 'hash', 'watype'))
wamf = sqldf("select * from wam where skew >= 0")
zipfwaabs = ggplot(duckdf("select * from wamf where patterncol like 'zipf'"), aes(x=factor(skew), y=value, color=device, shape=device, linetype=factor(watype, ordered=TRUE, levels=c("measured (OCP)", "implied")), group=interaction(device, variable))) +
	geom_point() +
	geom_line() +
	expand_limits(y=0) +
	geom_dl(aes(label=devicecol), method = list("last.qp", dl.trans(x = x + .2))) +
	scale_x_discrete(breaks=c(0, 0.2, 0.4, 0.6, 0.8, 1), labels=c('uniform', '0.2', '0.4', '0.6', '0.8', '1')) +
	ylab("WAF") +
	xlab("Zipf factor") +
	scale_linetype_manual(values = c('measured (OCP)' = "solid", 'implied' = "dashed")) +
	scale_color_discrete(guide = FALSE) +
	scale_shape_manual(values=1:9, guide = FALSE) +
	theme_bw() +
	theme(legend.position=c(0.3, 0.2), legend.title=element_blank())
zipfwaabs
CairoPDF("paper-zipf-wa-abs", width=width3, height=height3, bg = "transparent")
zipfwaabs
dev.off()
# PAPER: ZONES: relative
wanorm = sqldf("select *, 
							wa / (select wa 
								from wa i
								where i.device = o.device 
									and i.pattern = 'uniform' and i.patterncol = o.patterncol)
						as writefact
						from wa o
						")
pwanorm = ggplot(sqldf("select * from wanorm where patterncol like 'zones' and skew > 0"), aes(x=factor(skew), y=writefact, group=devicecol, color=devicecol, shape=devicecol, linetype=factor(watype, ordered=TRUE, levels=c("measured (OCP)", "implied")))) +
	geom_hline(yintercept = 1, linetype='dotted') +
	geom_line() +
	geom_point() +
	geom_dl(aes(label=devicecol), method = list("last.qp", dl.trans(x = x + .2))) +
	scale_y_continuous(limits=c(0.32, 1.42), breaks=seq(0.4, 1.4, 0.2)) +
	xlab("Two-Zones") +
	ylab("WAF normalized to uniform") +
	scale_x_discrete(breaks=c(0.5, 0.6, 0.7, 0.8, 0.9, 0.95), labels=c('uniform', '60/40', '70/30', '80/20', '90/10', '95/5')) +
	scale_shape_manual(values=1:9, guide = FALSE) +
	scale_color_discrete(guide = FALSE) +
	scale_linetype_manual(values = c('measured (OCP)' = "solid", 'implied' = "dashed")) +
	theme_bw() + 
	theme(legend.position=c(0.3, 0.2), legend.title=element_blank())
pwanorm
CairoPDF("paper-wa-zones-norm", width=width3, height=height3, bg = "transparent")
pwanorm
dev.off()
# PAPER: ZIPF: wa norm
pwanorm = ggplot(sqldf("select * from wanorm where patterncol like 'zipf' and skew >= 0"), aes(x=factor(skew), y=writefact, group=devicecol, color=devicecol, shape=devicecol, linetype=factor(watype, ordered=TRUE, levels=c("measured (OCP)", "implied")))) +
	geom_hline(yintercept = 1, linetype='dotted') +
	geom_line() +
	geom_point() +
	geom_dl(aes(label=devicecol), method = list("last.qp", dl.trans(x = x + .2))) +
	scale_y_continuous(limits=c(0.32, 1.42), breaks=seq(0.4, 1.4, 0.2)) +
	scale_x_discrete(breaks=c(0, 0.2, 0.4, 0.6, 0.8, 1), labels=c('uniform', '0.2', '0.4', '0.6', '0.8', '1')) +
	xlab("Zipf factor") +
	ylab("WAF normalized to uniform") +
	scale_linetype_manual(values = c('measured (OCP)' = "solid", 'implied' = "dashed")) +
	scale_color_discrete(guide = FALSE) +
	scale_shape_manual(values=1:9, guide = FALSE) +
	theme_bw() +
	theme(legend.position=c(0.3, 0.2), legend.title = element_blank())
pwanorm
CairoPDF("paper-zipf-wa-norm", width=width3, height=height3, bg = "transparent")
pwanorm
dev.off()


###################################################################################################
###################################################################################################

# read-only mixed workload

folder = "finalwa14"
prefix = "finalwa14rozipf"
i1 = read.csv(folder%+%"/iob-log-h0-"%+%prefix%+%".csv")
i2 = read.csv(folder%+%"/iob-log-i1-"%+%prefix%+%".csv")
i3 = read.csv(folder%+%"/iob-log-k4-"%+%prefix%+%".csv")
i4 = read.csv(folder%+%"/iob-log-k5-"%+%prefix%+%".csv")
i5 = read.csv(folder%+%"/iob-log-m0-"%+%prefix%+%".csv")
i6 = read.csv(folder%+%"/iob-log-mp0-"%+%prefix%+%".csv")
i7 = read.csv(folder%+%"/iob-log-s0-"%+%prefix%+%".csv")
i8 = read.csv(folder%+%"/iob-log-wd-"%+%prefix%+%".csv")
iobl = rbind(i1, i2, i3, i4, i5, i6, i7, i8)
iobl = duck("SELECT * exclude(pattern), CASE WHEN patternDetails LIKE '%f0 %' THEN 'zones-ro' ELSE pattern END AS pattern FROM iobl")
head(iobl)
#iobl = duckdf("select *, case when device like '/blk/i%' then '/blk/i' when device like '/blk/k%' then '/blk/k' when device like 'pm%' then 'pm' else device end as devicecol from iobl")
ioblunizones = duckdf("select *, 'zones' as patterncol from iobl where pattern like 'uniform'")
ioblunizonesro = duckdf("select *, 'ro' as patterncol from iobl where pattern like 'uniform'")
iobl = duckdf("select *, 
			  case when pattern like 'zipf%' then 'zipf'
			  when pattern like 'zones-ro%' then 'ro'
			  when pattern like 'zones%' then 'zones'
			  when pattern like 'uniform' then 'zipf'
		  else pattern end as patterncol from iobl ")
#duckdf("select distinct(skew) from iobl")
iobl = rbind(iobl, ioblunizones, ioblunizonesro)
iobl = duckdf("select *, 
				case  
					when pattern like '%uniform%' and patterncol like 'zones' then 0.5
					when pattern like '%uniform%' and patterncol like 'zipf' then 0
					when pattern like '%uniform%' and patterncol like 'ro' then 0
					when pattern like '%sequential%' then -1
					when pattern like '%zipf%' then cast(patterndetails as numeric)
					when patterndetails like '%s0.25 f0%' then 0.25
					when patterndetails like '%s0.2 f0%' then 0.2
					when patterndetails like '%s0.4 f0%' then 0.4
					when patterndetails like '%s0.6 f0%' then 0.6
					when patterndetails like '%s0.8 f0%' then 0.8
					when patterndetails like '%s0.9 f0%' then 0.9
					when patterndetails like '%s0.6%' then 0.6
					when patterndetails like '%s0.5%' then 0.5
					when patterndetails like '%s0.75 f0%' then 0.75
					when patterndetails like '%s0.7%' then 0.7
					when patterndetails like '%s0.8%' then 0.8
					when patterndetails like '%s0.95%' then 0.95
					when patterndetails like '%s0.9%' then 0.9
					else -2
					end as skew	
				from iobl")
#duckdf("select skew, string_agg(distinct(pattern)), string_agg(distinct(patterncol)) from iobl group by skew")
unique(iobl$patterndetails)
unique(iobl$skew)
unique(iobl$device)
iobl$devicecol = iobl$device
iobl$devicecol <- str_replace(iobl$devicecol, ".*/h.*", "h")
iobl$devicecol <- str_replace(iobl$devicecol, ".*/intel.*", "i")
iobl$devicecol <- str_replace(iobl$devicecol, ".*/k.*", "k")
iobl$devicecol <- str_replace(iobl$devicecol, ".*/mp.*", "mw")
iobl$devicecol <- str_replace(iobl$devicecol, ".*/m.*", "m")
iobl$devicecol <- str_replace(iobl$devicecol, ".*/s.*", "s")
iobl$devicecol <- str_replace(iobl$devicecol, ".*/wd.*", "w")
devices = read.csv("devices.csv")
colnames(iobl) <- make.unique(tolower(colnames(iobl)), sep = "_s")
c(colnames(iobl))
#iobl$writesmbs = as.double(iobl$writeiops) * iobl$bs / 1024 / 1024
#head(iobl$writesmbs)
iobl = duckdf("select * from iobl join devices using (devicecol)")
duckdf("select count(distinct(device)) from iobl")
ioblnoavg = duckdf("select *, 
				   avg(writesMBs) OVER (
						partition by device, pattern, patterncol, patterndetails
						order by time ASC ROWS BETWEEN 10 PRECEDING AND 10 FOLLOWING) as winavg
				   from iobl")
head(ioblnoavg)
unique(ioblnoavg$devicecol)
head(duck("select time,device,devicecol from iobl where device like '%i4i%' order by random()"), 2)
head(duck("select time,device,devicecol,winavg from ioblnoavg order by random()"), 2)
# filter noise 
iobl = duckdf("select * from ioblnoavg where time <> 0 and writesMBs > 0.5*winavg and writesMBs < 1.5*winavg") 
unique(iobl$devicecol)
cat('smooth data, removed: ') 
nrow(ioblnoavg) - nrow(iobl)
iobl = duckdf("select *, cast(seqWriteMBs*1e6/1024/1024 as double) / writesMBs as impliedWA, max(writesMBs) over (partition by device) * 1.0 / writesMBs as impliedWA2 from iobl")
colnames(iobl)
# absolute plots
ssds = "('h', 's', 'm', 'w', 'i')"
ioblag = sqldf("select avg(nullif(wa, 0)) as wa, avg(impliedWA) as impliedWA, avg(impliedWA2) as impliedWA2, avg(writesMBs) as writesMBs, avg(readsMBs) as readsMBs, min(time) as time, max(writestotal) as writestotal, device, deviceCol, pattern, patterncol, skew, patternDetails, bs, hash, capacityGB
			   from iobl
			   where pattern <> 'sequential' and (devicecol in "%+%ssds%+%" )
			   group by time/100, device, deviceCol, pattern, patterncol, skew, patternDetails, hash, capacityGB")
write.csv(ioblag, "paper-read-only.csv")
ioblag = read.csv("paper-read-only.csv")
ioblagm = melt(ioblag, id=c('hash', 'pattern', 'patterncol', 'skew', 'patterndetails', 'device', 'devicecol', 'time', 'writestotal', 'bs', 'capacityGB'))
ioblagmf = sqldf("select *,
					value / max(value) over (partition by device, variable, patterncol) as normvalue
				 from ioblagm 
				 where (variable like 'writesMBs' or variable like 'wa')
					and (patterncol = 'ro')
					--- and patterncol like 'zipf'")

#ioblagmfn = duck("select * from ioblagmf group by ")
ggplot(ioblagmf, aes(x=writestotal*bs/1024/1024/1024/(capacityGB*1e9/1024/1024/1024), y=value, color=interaction(skew), shape=variable)) +
	geom_line(alpha=0.9) +
	#geom_point(alpha=0.5, size=0.75) +
	facet_grid(interaction(patterncol, variable) ~ device, scales="free") +
	#ylim(0, 7) +
	theme_bw()

wa = sqldf("select device, deviceCol, pattern, patterncol, skew, patternDetails, bs, hash,
		   avg(wa) as wa,
		   avg(writesMBs) avgWritesMBs,
		   avg(impliedWA) as impliedWA,
		   avg(impliedWA2) as impliedWA2,
		   max(writestotal)
	  from ioblag o
	   where 
		writestotal > (select max(writestotal) from ioblag ii where ii.device = o.device and ii.hash = o.hash)*0.6 and
		writestotal < (select max(writestotal) from ioblag ii where ii.device = o.device and ii.hash = o.hash)*0.9
	  group by device, deviceCol, pattern, patterncol, skew, patternDetails, bs, hash")
head(wa)
wa = sqldf("select device, deviceCol, pattern, patterncol, skew, patternDetails, bs, hash,
			ifnull(wa, impliedWA) as wa, case when wa is null then 'implied' else 'measured (OCP)' end as watype
		from wa")
wam = melt(wa, id = c('device', 'devicecol', 'pattern', 'patterncol', 'skew', 'patterndetails', 'bs', 'hash', 'watype'))
wamf = sqldf("select * from wam where skew >= 0")
wamff = duckdf("select * 
					from wamf 
					where 
						patterncol like 'ro' and 
						devicecol not in ('mw')")
ro = ggplot(wamff, aes(x=factor(skew), y=value, color=devicecol, shape=devicecol, linetype=factor(watype, ordered=TRUE, levels=c("measured (OCP)", "implied")), group=interaction(device, variable))) +
	geom_point() +
	geom_line() +
	expand_limits(y=0) +
	scale_x_discrete(breaks=c(0, 0.2, 0.4, 0.6, 0.8, 0.9), labels=c('uniform', '20%', '40%', '60%', '80%', '90%')) +
	#scale_y_continuous(expand=expansion(add = c(0,5))) +
	ylab("WAF") +
	xlab("Read-Only Percentage") +
	geom_dl(aes(label=devicecol), method = list(dl.trans(x = x + .2), "last.points")) +
	scale_linetype_manual(values = c('measured (OCP)' = "solid", 'implied' = "dashed")) +
	scale_colour_discrete(guide = FALSE) +
	scale_shape_manual(values=1:9, guide = FALSE) +
	#facet_grid(~ patterncol) +
	theme_bw() + 
	theme(legend.position=c(0.3,0.2), legend.title=element_blank())
ro
width3=4
height3=3.3*3/4
CairoPDF("paper-ro-wa", width=width3, height=height3, bg = "transparent")
ro
dev.off()


###################################################################################################
###################################################################################################

# ZNS  active zones

folder = "finalwa30"
pref = "finalwa30znstrimzonesize8GBz1to32"
i1 = read.csv(folder%+%"/iob-log-i1-"%+%pref%+%".csv")
i2 = read.csv(folder%+%"/iob-log-k4-"%+%pref%+%".csv")
i3 = read.csv(folder%+%"/iob-log-m0-"%+%pref%+%".csv")
i4 = read.csv(folder%+%"/iob-log-s0-"%+%pref%+%".csv")
i5 = read.csv(folder%+%"/iob-log-wd-"%+%pref%+%".csv")
#i6 = read.csv(folder%+%"/iob-log-i1-"%+%pref%+%".csv")
#i7 = read.csv(folder%+%"/iob-log-k5-"%+%pref%+%".csv")
i8 = read.csv(folder%+%"/iob-log-h0-"%+%pref%+%".csv")
i9 = read.csv(folder%+%"/iob-log-mp0-"%+%pref%+%".csv")
iobl = rbind(i1, i2, i3, i4, i5, i8, i9)
iobl = sqldf("select *, case when device like '/blk/i%' then '/blk/i' when device like '/blk/k%' then '/blk/k' when device like 'pm%' then 'pm' else device end as devicecol from iobl")
head(iobl, 2)
unique(iobl$device)
iobl$devicecol = iobl$device
iobl$devicecol <- str_replace(iobl$devicecol, ".*/h.*", "h")
iobl$devicecol <- str_replace(iobl$devicecol, ".*/intel.*", "i")
iobl$devicecol <- str_replace(iobl$devicecol, ".*/k.*", "k")
iobl$devicecol <- str_replace(iobl$devicecol, ".*/mp.*", "mw")
iobl$devicecol <- str_replace(iobl$devicecol, ".*/m.*", "m")
iobl$devicecol <- str_replace(iobl$devicecol, ".*/s.*", "s")
iobl$devicecol <- str_replace(iobl$devicecol, ".*/wd.*", "w")
unique(iobl$devicecol)
devices = read.csv("devices.csv")
colnames(iobl) <- make.unique(tolower(colnames(iobl)), sep = "_s")
c(colnames(iobl))
sqldf("select distinct(device) from iobl")
iobl = sqldf("select * from iobl join devices using (devicecol)")
sqldf("select distinct(device) from iobl")
iobl = sqldf("select *, length(patternDetails)/36 as patterncol,  seqWriteMBs*1e6/1024/1024 * 1.0 / writesMBs as impliedWA from iobl")
# filter noise 
ioblnoavg = duckdf("select *, avg(writesMBs) OVER (partition by hash, device, pattern, patterncol, patterndetails order by time ASC ROWS BETWEEN 10 PRECEDING AND 10 FOLLOWING) as winavg from iobl ")
print("smooth data, removed by noise filter: "%+% duckdf("select count(*) from ioblnoavg where writesMBs < 0.5*winavg or writesMBs > 1.5*winavg")) 
iobl = duckdf("select * from ioblnoavg where time <> 0 and writesMBs > 0.5*winavg and writesMBs < 1.5*winavg") 
nrow(ioblnoavg) - nrow(iobl)
head(iobl)
# time vs absolute plots
ssds = "('/blk/h0', '/blk/s0', '/blk/intel0', '/blk/m0')"
ioblag = sqldf("select avg(nullif(wa, 0)) as wa, avg(impliedWA) as impliedWA, avg(writesMBs) as writesMBs, avg(readsMBs) as readsMBs,
					time/250*250 as time, max(writestotal) as writestotal,
					device, deviceCol, capacityGB, pattern, patterncol, patterndetails, bs, hash
			   from iobl
			   where pattern <> 'sequential' and (true or device in "%+%ssds%+%")
			   group by time/250, device, deviceCol, pattern, patternDetails, bs, hash")
write.csv(ioblag, "paper-zns-zonecnt.csv")

ioblag = read.csv("paper-zns-zonecnt.csv")
ioblagc = sqldf("select ifnull(wa, impliedWA) as wa, case when wa is null then 'implied' else 'measured (OCP)' end as watype,
				writesMBs, readsMBs, time, writestotal, device, deviceCol, capacityGB, pattern, patterncol, patterndetails, bs, hash
				from ioblag")
ioblagm = melt(ioblagc, id=c('hash', 'pattern', 'patterncol', 'patterndetails', 'device', 'devicecol', 'capacityGB', 'time', 'writestotal', 'watype', 'bs'))
ioblagmf = sqldf("select * from ioblagm where variable like '%write%' or variable like '%wa%'")
ioblag$activezones = regmatches(ioblag$patterndetails, regexpr("(?<=activeZones: )\\d+", ioblag$patterndetails, perl = TRUE))
#duckdf("select device, activezones, count(*) cnt, max(time) maxtime from ioblag group by device, activezones order by cnt, activezones")
duckdf("select device, activezones, hash, max(time) maxtime 
	   from ioblag
	   where device like '%mp0'
	   group by device, activezones, hash  order by hash, device, activezones")
ggplot(duckdf("select * from ioblagmf where true"), aes(x=(1.0*writestotal*bs)/(1024^4), y=value, color=interaction(bs/1024, patterndetails), group=hash, linetype=watype)) +
	geom_line(alpha=0.9) +
	facet_grid(variable	~ device, scales='free') +
	theme_bw()
# PAPER: SEQ-ZONES: tp vs. skew absolute 
ioblagg = sqldf("select device, deviceCol, pattern, patternDetails, bs, hash, watype, avg(wa) as wa,  max(time) as maxtime
	  from ioblagc o
	   where 
		time > (select max(time) from ioblagc ii where ii.device = o.device and ii.hash = o.hash)*0.5 and
		time < (select max(time) from ioblagc ii where ii.device = o.device and ii.hash = o.hash)*0.95
	  group by device, deviceCol, pattern, patternDetails, bs, hash, watype
	  order by watype asc")
head(ioblagg)
ioblagg$activezones = as.numeric(regmatches(ioblagg$patterndetails, regexpr("(?<=activeZones: )\\d+", ioblagg$patterndetails, perl = TRUE)))
wauniform = duck("select * from wa where pattern == 'uniform' and skew == 0")
ioblaggf = sqldf("select * from ioblagg where devicecol not in ('k', 'mw') and activezones <= 16")
wauniform2 = duck("select w.*, w.wa / i.wa as wafactor
		from wauniform w join ioblaggf i  using (devicecol)
		")
ioblaggf_znscnt = ioblaggf

znscnt_plot = ggplot(ioblaggf_znscnt,
					aes(x=reorder(activezones, as.numeric(activezones)), y=wa,
						group=interaction(device, watype), color=devicecol, 
						linetype=factor(watype, ordered=TRUE, levels=c("measured (OCP)", "implied")),
						shape=devicecol)) +
	geom_line() +
	geom_point() +
	geom_dl(aes(label=devicecol), method = list("last.qp", dl.trans(x = x + .2))) +
	#geom_point(wauniform2, mapping=aes(x="uniform", y=wa, color=devicecol, shape=devicecol)) +
	#geom_dl(wauniform2, mapping=aes(x="uniform", y=wa, 
	#					#label=(devicecol%+%" ("%+%round(wafactor,1)%+%"x)")),
	#						label=devicecol),
	#					method = list("last.qp", dl.trans(x = x + .2))) +
	scale_x_discrete() +
	scale_y_continuous(breaks=seq(0,8,2), limits=c(0, 7.05))+
	#expand_limits(x=9.95, y=6.1) +
	xlab("# Active Write Zones") +
	ylab("WAF") +
	scale_linetype_manual(values = c('measured (OCP)' = "solid", 'implied' = "dashed")) +
	scale_shape_manual(values=1:9, guide=FALSE) +
	scale_color_discrete(guide=FALSE) +
	expand_limits(y=0) +
	theme_bw() +
	theme(legend.position='none') +
	#theme(legend.position='none', legend.position=c(0.2, 0.85), legend.title=element_blank()) +
	theme(axis.text.y = element_blank()) +
	theme(axis.title.y=element_blank())
znscnt_plot	
width3=1.8
height3=1.6
CairoPDF("paper-zns-zonescnt", width=width3, height=height3, bg = "transparent")
znscnt_plot
dev.off()

dev.new()

###################################################################################################
###################################################################################################

# ZNS zone size ^^^^ requires wa ^^^^^ and iobl from above ^^^

folder = "finalwa28"
pref = "finalwa28znstrim"
i1 = read.csv(folder%+%"/iob-log-i1-"%+%pref%+%".csv")
i2 = read.csv(folder%+%"/iob-log-k4-"%+%pref%+%".csv")
i3 = read.csv(folder%+%"/iob-log-m0-"%+%pref%+%".csv")
i4 = read.csv(folder%+%"/iob-log-s0-"%+%pref%+%".csv")
i5 = read.csv(folder%+%"/iob-log-wd-"%+%pref%+%".csv")
#i6 = read.csv(folder%+%"/iob-log-i1-"%+%pref%+%".csv")
#i7 = read.csv(folder%+%"/iob-log-k5-"%+%pref%+%".csv")
i8 = read.csv(folder%+%"/iob-log-h0-"%+%pref%+%".csv")
i9 = read.csv(folder%+%"/iob-log-mp0-"%+%pref%+%".csv")
###################################################################################################
iobl = rbind(i1, i2, i3, i4, i5, i8, i9) # << add iobl if needed for missing values
###################################################################################################
iobl = sqldf("select *, case when device like '/blk/i%' then '/blk/i' when device like '/blk/k%' then '/blk/k' when device like 'pm%' then 'pm' else device end as devicecol from iobl")
head(iobl, 2)
unique(iobl$device)
iobl$devicecol = iobl$device
iobl$devicecol <- str_replace(iobl$devicecol, ".*/h.*", "h")
iobl$devicecol <- str_replace(iobl$devicecol, ".*/intel.*", "i")
iobl$devicecol <- str_replace(iobl$devicecol, ".*/k.*", "k")
iobl$devicecol <- str_replace(iobl$devicecol, ".*/mp.*", "mw")
iobl$devicecol <- str_replace(iobl$devicecol, ".*/m.*", "m")
iobl$devicecol <- str_replace(iobl$devicecol, ".*/s.*", "s")
iobl$devicecol <- str_replace(iobl$devicecol, ".*/wd.*", "w")
unique(iobl$devicecol)
devices = read.csv("devices.csv")
colnames(iobl) <- make.unique(tolower(colnames(iobl)), sep = "_s")
c(colnames(iobl))
sqldf("select distinct(device) from iobl")
iobl = sqldf("select * from iobl join devices using (devicecol)")
sqldf("select distinct(device) from iobl")
iobl = sqldf("select *, length(patternDetails)/36 as patterncol,  seqWriteMBs*1e6/1024/1024 * 1.0 / writesMBs as impliedWA from iobl")
# filter noise 
ioblnoavg = duckdf("select *, avg(writesMBs) OVER (partition by hash, device, pattern, patterncol, patterndetails order by time ASC ROWS BETWEEN 10 PRECEDING AND 10 FOLLOWING) as winavg from iobl ")
print("smooth data, removed by noise filter: "%+% duckdf("select count(*) from ioblnoavg where writesMBs < 0.5*winavg or writesMBs > 1.5*winavg")) 
iobl = duckdf("select * from ioblnoavg where time <> 0 and writesMBs > 0.5*winavg and writesMBs < 1.5*winavg") 
nrow(ioblnoavg) - nrow(iobl)
head(iobl)
# time vs absolute plots
ssds = "('h', 's', 'm', 'w', 'i')"
ioblag = sqldf("select avg(nullif(wa, 0)) as wa, avg(impliedWA) as impliedWA, avg(writesMBs) as writesMBs, avg(readsMBs) as readsMBs,
					time/250*250 as time, max(writestotal) as writestotal,
					device, deviceCol, capacityGB, pattern, patterncol, patterndetails, bs, hash 
			   from iobl
			   where pattern <> 'sequential' and (devicecol in "%+%ssds%+%")
			   group by time/250, device, deviceCol, pattern, patternDetails, bs, hash")
ioblag$pagesperzone = regmatches(ioblag$patterndetails, regexpr("(?<=pagesPerZone: )\\d+", ioblag$patterndetails, perl = TRUE))
ioblag$zonesize = as.numeric(ioblag$pagesperzone) * 512 * 1024 / 1024 / 1024 / 1024
ioblag$activezones = as.numeric(regmatches(ioblag$patterndetails, regexpr("(?<=activeZones: )\\d+", ioblag$patterndetails, perl = TRUE)))
head(ioblag)
write.csv(ioblag, "paper-zns-zonesize.csv")
ioblag = read.csv("paper-zns-zonesize.csv")
ioblagc = sqldf("select ifnull(wa, impliedWA) as wa, case when wa is null then 'implied' else 'measured (OCP)' end as watype,
				writesMBs, readsMBs, time, writestotal, device, deviceCol, capacityGB, pattern, patterncol, patterndetails, pagesperzone, zonesize, activezones, bs, hash
				from ioblag")
ioblagm = melt(ioblagc, id=c('hash', 'pattern', 'patterncol', 'patterndetails', 'device', 'devicecol', 'capacityGB', 'time', 'writestotal', 'watype', 'bs', 'pagesperzone', 'zonesize', 'activezones'))
ioblagmf = sqldf("select * from ioblagm where (variable like '%write%' or variable like '%wa%') and pagesperzone not in (2154, 2172, 5285) and activezones = 1")
#ggplot(ioblagmf, aes(x=time, y=value, color=interaction(bs/1024, patterndetails), group=hash)) +
ggplot(ioblagmf, aes(x=(1.0*writestotal*bs)/(1024^3)/(capacityGB*10^9/1024^3), y=value, color=interaction(bs/1024, patterndetails), group=hash)) +
	geom_line(alpha=0.9) +
	facet_grid(variable	~ device, scales='free') +
	#facet_grid(variable	~ device) +
	expand_limits(y=0) +
	theme_bw()
# PAPER: SEQ-ZONES: tp vs. skew absolute 
ioblagg = sqldf("select device, deviceCol, pattern, patternDetails, pagesperzone, zonesize, activezones, bs, hash, watype, avg(wa) as wa,  max(time)
	  from ioblagc o
	   where 
		time > (select max(time) from ioblagc ii where ii.device = o.device and ii.hash = o.hash)*0.75 and
		time < (select max(time) from ioblagc ii where ii.device = o.device and ii.hash = o.hash)*0.95
	  group by device, deviceCol, pattern, patternDetails, bs, hash, watype
	  order by watype asc")
wauniform = duck("select * from wa where pattern == 'uniform' and skew == 0")
ioblaggf = sqldf("select * from ioblagg where devicecol not in ('mw', 'k') and pagesperzone not in (2154, 2172, 5285) and activezones = 1")
wauniform2 = duck("select w.*, w.wa / i.wa as wafactor
		from wauniform w join ioblaggf i  using (devicecol)
		")
ioblaggf_znssize = ioblaggf

znssize_plot = ggplot(ioblaggf_znssize,
					aes(x=reorder(zonesize, as.numeric(zonesize)), y=wa,
						group=interaction(device, watype), color=devicecol, 
						linetype=factor(watype, ordered=TRUE, levels=c("measured (OCP)", "implied")),
						shape=devicecol)) +
	geom_line() +
	geom_point() +
	geom_dl(aes(label=devicecol), method = list("last.qp", dl.trans(x = x + .2))) +
	#geom_point(wauniform2, mapping=aes(x="uniform", y=wa)) +
	#geom_dl(wauniform2, mapping=aes(x="uniform", y=wa, 
#							#label=(devicecol%+%" ("%+%round(wafactor,1)%+%"x)")),
#						label=devicecol),
#						method = list("last.qp", dl.trans(x = x + .2))) +
	scale_x_discrete() +
	scale_y_continuous(breaks=seq(0,8,2), limits=c(0, 7.05))+
	#expand_limits(x=9.95, y=6.1) +
	xlab("Zone Size [GB]") +
	ylab("WAF") +
	scale_linetype_manual(values = c('measured (OCP)' = "solid", 'implied' = "dashed")) +
	scale_shape_manual(values=1:9, guide=FALSE) +
	scale_color_discrete(guide=FALSE) +
	expand_limits(y=0) +
	theme_bw() +
	#theme(legend.position=c(0.7, 0.85), legend.title=element_blank())
	theme(legend.position='none')
znssize_plot
width3=2.2
height3=width3/1.2
CairoPDF("paper-zns-size-wa", width=width3, height=height3, bg = "transparent")
znssize_plot
dev.off()
width3=4.2
height3=2.25
CairoPDF("paper-zns", width=width3, height=height3, bg = "transparent")
grid.arrange(znssize_plot, znscnt_plot, nrow=1, ncol=2, widths=c(1, 0.87))
dev.off()

dev.new()

###################################################################################################
###################################################################################################
###################################################################################################
###################################################################################################

iobl = loadIobFiles("finalop02b", c("iob-log-m0op02.csv", "iob-log-mp0op02.csv"))
iobl2 = doLogTransformations(iobl)
iobl3 = iobMelt(iobl2)
ioblm = iobAvgOverTime(iobl3)
write.csv(ioblm, "paper-op.csv")
ioblm = read.csv("paper-op.csv")

# time check
iobl2f = duck("select * from iobl2 where pattern == 'uniform'")
ggplot(iobl2f, aes(x=time, y=writesmbs, color=devicecol, linetype=factor(filesizegib))) +
	geom_line() +
	xlim(0, 2000) +
	theme_bw()

# PAPER: OP plot
ioblmf = duckdf("select  * from ioblm where variable like 'wa' and pattern like 'uniform'")
head(ioblmf)
pop = ggplot(ioblmf, aes(x=filesizegib*1024*1024/1e6, y=value, color=devicecol, shape=devicecol)) +
	geom_line(alpha=0.9) +
	#geom_point(duck("select * from ioblmf where devicecol == 'm'"), mapping=aes(x=filesizegib, y=value), alpha=1) +
	geom_point() +
	annotate("text", label = "Micron 7450 PRO\n\"read-intensive\"", x=600, y=2.45, color="#F8766D") + 
	annotate("text", label = "Micron 7450 MAX\n\"mixed-used\"",     x=740, y=0.9, color="#00BFC4") + 
	ylab("WAF") + 
	xlab("SSD fill size [GB]") +
	expand_limits(y=0) +
	scale_shape_manual(values=c(5, 4), guide = FALSE) +
	theme_bw() +
	theme(legend.position = "none") + 
	theme()
pop
widthInlineHalf=2.5
heightInlineHalf=2
CairoPDF("paper-op", width=widthInlineHalf, height=heightInlineHalf, bg = "transparent")
pop
dev.off()


