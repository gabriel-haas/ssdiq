library(ggplot2)
library(sqldf)
library(Cairo)
library(gridExtra)
library(grid)
library(reshape2)
library(stringr)
library(directlabels)
#library(zoo)
'%+%' <- function(x, y) paste0(x,y)
# necessary for wa plots outliers 

removeOutliers = function() {
	Ioblnoavg = duck("select *, avg(writesMBs) OVER (partition by device, pattern, patterncol, patterndetails order by time ASC ROWS BETWEEN 10 PRECEDING AND 10 FOLLOWING) as winavg from iobl ")
	# filter noise 
	iobl = duck("select * from ioblnoavg where time <> 0 and writesMBs > 0.5*winavg and writesMBs < 1.5*winavg") 
	cat('smooth data, removed: ') 
	nrow(ioblnoavg) - nrow(iobl)
}
doLogTransformations = function(iobl) {
	iobl = subset(iobl, select=-c(stat))
	unique(iobl$device)
	iobl$devicecol = iobl$device
	iobl$devicecol <- str_replace(iobl$devicecol, ".*/h.*", "h")
	iobl$devicecol <- str_replace(iobl$devicecol, ".*/intel.*", "i")
	iobl$devicecol <- str_replace(iobl$devicecol, ".*/k.*", "k")
	iobl$devicecol <- str_replace(iobl$devicecol, ".*/mp.*", "mw")
	iobl$devicecol <- str_replace(iobl$devicecol, ".*/m.*", "m")
	iobl$devicecol <- str_replace(iobl$devicecol, ".*/s.*", "s")
	iobl$devicecol <- str_replace(iobl$devicecol, ".*/wd.*", "w")
	iobl$devicecol <- str_replace(iobl$devicecol, ".*pm.*", "sp")
	unique(iobl$devicecol)
	devices = read.csv("devices.csv")
	iobl = sqldf("select * from iobl join devices using (devicecol)")
	colnames(iobl) <- tolower(colnames(iobl))
	colnames(iobl) <- make.unique(colnames(iobl), sep = "_s")
	return(iobl)
}
doStatsTransformations = function(iobl) {
	iobl = subset(iobl, select=-c(r,s,w,c))
	colnames(iobl) <- tolower(colnames(iobl))
	return(iobl)
}
loadIobFiles = function(folder, filenames) {
	iob = data.frame()	
	for (f in filenames) {
		fn = folder%+%"/"%+%f
		cat("loading ") ; print(fn)
		i = read.csv(fn)
		iob = rbind(iob, i)
	}
	return(iob)
}
loadIobFilesMatching = function(folder, type, patterns) {
	if (!dir.exists(folder)) {
		stop("Specified folder does not exist.")
	}
	all_files <- list.files(folder)
	matching_files <- character(0)
	all_files <- c(matching_files, grep(type, all_files, value = TRUE))
	for (pattern in patterns) {
		matching_files <- c(matching_files, grep(pattern, all_files, value = TRUE))
	}
	matching_files <- unique(matching_files)
	return(loadIobFiles(folder, matching_files))
}
iobLogIdColumns = c('hash', 'type', 'time', 'exacttime', 'device', 'devicecol', 'filesizegib', 'fill', 'usedfilesize', 'pattern', 'patterndetails',
					'rate', 'exprate', 'writepercent', 'bs','model', 'comment', 'capacitygb', 'pciegen', 'sequential', 'seqreadmbs', 'seqwritembs',
					'rndwriteiops', 'rndreadiops', 'dwpd', 'readlatencyus', 'writelatencyus')
iobMelt = function(iobl) {
	m <- melt(iobl, id =iobLogIdColumns)
	return(m)
}
iobStatIdColumns = c('hash','id','time', 'timediff', 'device', 'filesizegib',
					 'pattern', 'patterndetails', 'rate', 'threadrate', 'threadstatsinterval', 'exprate', 
					 'writepercent', 'writemibs', 'readmibs', 'writes', 'reads', 'fdatasyncs')
iobLogStatsMelt = function(iobl) {
	#s = setdiff(iobLogIdColumns, c('exacttime', 'type')) #subset(iobLogIdColumns, select=-c(exacttime))
	#print(s)
	m <- melt(iobl, id = c(iobStatIdColumns))
	return(m)
}
iobAvgOverTime = function(ioblmx, aggFunction, aggFunctionPart2) {
	if (missing(aggFunction)) {
		aggFunction = "AVG("
		aggFunctionPart2 = ")"
	}
	cols = "hash, device, devicecol, filesizegib, fill, usedfilesize, pattern, patterndetails, rate, writepercent, bs, 
			   model, comment, capacitygb, pciegen, sequential, seqreadmbs, seqwritembs, rndwriteiops, rndreadiops, dwpd, readlatencyus, writelatencyus, variable"
	query = "select "%+%cols%+%", "%+%aggFunction%+%"cast(value as double)"%+%aggFunctionPart2%+%" as value
		  from ioblmx o
		   where 
			time > (select max(time) from ioblmx ii where ii.device = o.device and ii.hash = o.hash)*0.6 and
			time < (select max(time) from ioblmx ii where ii.device = o.device and ii.hash = o.hash)*0.99
		  group by "%+%cols
	#print(query)
	wa = duck(query)
	return(wa)
}
