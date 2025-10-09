library('duckdb')
library('ggplot2')
library('reshape2')
library(gridExtra)
library(cowplot)
library(grid)
library(Cairo)
'%+%' <- function(x, y) paste0(x,y)
duck <- function(query) {
 if (!exists("duckdbcon")) {
   duckdbcon <<- dbConnect(duckdb(environment_scan = TRUE))
	dbExecute(duckdbcon, "INSTALL json")
	dbExecute(duckdbcon, "LOAD json")
 }
 dbFetch(dbSendQuery(duckdbcon, query))
}
#dbExecute(duckdbcon, "INSTALL json")
#dbExecute(duckdbcon, "LOAD json")


#s = read.csv("../build/switch-mdc-2a.csv")
#s = read.csv("../build/switch-2a-tt-wh.csv")
s = read.csv("../build/switch-r03.csv")
#s = read.csv("../build/simoutf.csv")
sm = melt(s, id=c("sim", "hash", "rep", "time", "capacity", "erase", "pagesize", "pattern", "skew", "zones", "alpha", "beta", "ssdFill", "gc", 'writeheads', "timestamps","mdcbatch", "opthistsize", "ignore"))
head(s)
smf = duck("select * from sm where (gc not like '2a%' or (writeheads >= 0 and timestamps >= 1))")
ggplot(smf, aes(x=rep, y=value, color=interaction(gc), groups=interaction(gc,mdcbatch,writeheads,timestamps), shape=factor(timestamps), linetype=factor(mdcbatch))) +
	   geom_line() +
	   #geom_point() +
	   #scale_y_continuous(limits=c(2, 3))+
	   expand_limits(y=0) +
	   coord_cartesian(ylim=c(2, 4)) +
	   facet_grid(~ variable) +
	   theme_bw()


s = duck("SELECT * FROM read_csv_auto('../build/sim_*.csv')")
sm = melt(s, id=c("sim", "hash", "ssdwrites", "rep", "time", "capacity", "erase", "pagesize", "pattern", "skew", "zones", "alpha", "beta", "ssdFill", "gc", 'writeheads', "timestamps","mdcbatch", "opthistsize","prefix"))
head(s)
smf = duck("select * from sm where (gc not like '2a%' or (writeheads >= 0 and timestamps >= 1)) order by hash")
smf$color_key <- interaction(smf$gc, smf$writeheads, smf$timestamps, smf$mdcbatch, smf$hash, smf$prefix)
smf$color_key <- factor(smf$color_key, levels = unique(smf$color_key))
head(smf)
smfa = duck("select sim, hash, max(ssdwrites) as ssdwrites, max(rep) as rep, max(time) as time, capacity, erase, pagesize, pattern, skew, zones, alpha, beta, ssdFill, gc, writeheads, timestamps, mdcbatch, opthistsize, prefix, variable, avg(value) as value, color_key 
	 from smf where variable='runningWAF'
	 group by sim, hash, (ssdwrites / 0.01)::int, capacity, erase, pagesize, pattern, skew, zones, alpha, beta, ssdFill, gc, writeheads, timestamps, mdcbatch, opthistsize, prefix, variable, color_key")
head(smfa)
smfa2 = duck("select * from smf where variable <> 'runningWAF' union all select * from smfa")
ggplot(duck("select * from smfa2 where variable in ('runningWAF', 'cumulativeWAF')"), aes(x=ssdwrites, y=value, color=color_key, groups=interaction(gc,mdcbatch,writeheads,timestamps), shape=factor(timestamps), linetype=color_key)) +
	   geom_line() +
	   #geom_point() +
	   #expand_limits(y=0) +
	   coord_cartesian(ylim=c(2, 4)) +
	   coord_cartesian(ylim=c(2, 6), xlim=c(48, 60)) +
	   #coord_cartesian(xlim=c(0, 20)) +
	   #coord_cartesian(xlim=c(48, 60)) +
	   facet_grid( ~ variable) +
	   theme_bw()


l = duck("SELECT row_number() over (order by access) idx, * FROM read_csv_auto('../build/bla.log') order by idx asc")

duck("select count(*) from l group by access > 500")

head(l)
tail(l)
ggplot(l, aes(x=idx, y=access)) +
	geom_line() +
	geom_point() +
	#scale_y_log10()+
	#coord_cartesian(xlim=c(9400, 9600)) +
	expand_limits(y=0) +
	theme_bw()

