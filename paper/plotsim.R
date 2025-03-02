# Required libraries
library(ggplot2)
library(sqldf)
library(Cairo)
library(reshape2)
library(directlabels)
library(RColorBrewer)
library(geomtextpath)

# PAPER: sim plots

palette_colors = c("black", "gray20", "gray40", "gray60", "gray80")

width3=2.8
height3=2.475
data <- read.csv("plotzone.csv")
data_long <- melt(data, id.vars = "zipf", variable.name = "method", value.name = "value")
data_long$method <- gsub("^X", "", data_long$method)
data_long$method <- factor(data_long$method, levels = c("greedy", "kgreedy", "2Rgreedy", "2RFIFO", "optimal"))
# PAPER: sim normalized 
wa = data_long 
wa$skew = wa$zipf
wa$wa = wa$value
head(wa)
wanorm = sqldf("select *,
						wa / (select wa
							from wa i
							where i.method = o.method
								and i.skew = 0.5)
					as wafact
					from wa o
					")
head(wanorm)
psimnorm  <- ggplot(sqldf("select * from wanorm where method not in ('kgreedy')"), aes(x=factor(zipf), y=wafact, color=method, group=method)) +
  geom_line(alpha = 0.9) +
  #geom_dl(aes(label=method), method = list("last.qp", dl.trans(x = x + .2))) +
  geom_point() +
  annotate("text", x=5, y=1.35, angle=30,  label = "greedy", color = palette_colors[1]) +
  annotate("text", x=5.5, y=1.13, angle=0,  label = "2R-G", color = palette_colors[2]) +
  annotate("text", x=5.58, y=0.63,  angle=-25, label = "2R-F", color = palette_colors[3]) +
  annotate("text", x=5, y=0.38,  angle=-25, label = "Opt", color = palette_colors[4]) +
  annotate("label", label = "simulator ", x = 1.75, y = 0.35, 
           fill = "white", color = "black", label.size = NA) +  # No border
  scale_x_discrete(breaks=c(0.5, 0.6, 0.7, 0.8, 0.9, 0.95), labels=c('uniform', '60/40', '70/30', '80/20', '90/10', '95/5')) +
  scale_y_continuous(limits=c(0.32, 1.42), breaks=seq(0.4, 1.4, 0.2)) +
  scale_color_manual(values = palette_colors) +
  xlab("Zones") +
  ylab("Relative WAF") +
  theme_bw() +
  theme(axis.text.y = element_blank()) +
  theme(legend.position = 'none', legend.margin = margin(t = -0, unit = 'cm'), axis.title.y=element_blank())
psimnorm
CairoPDF("zone-sim-norm", width=width3, height=height3, bg = "transparent")
psimnorm
dev.off()
###################################################################################################
data <- read.csv("plotzipf.csv")
data_long <- melt(data, id.vars = "zipf", variable.name = "method", value.name = "value")
data_long$method <- gsub("^X", "", data_long$method)
data_long$method <- factor(data_long$method, levels = c("greedy", "kgreedy", "2Rgreedy", "2RFIFO", "optimal"))
wa = data_long 
wa$skew = wa$zipf
wa$wa = wa$value
head(wa)
wanorm = sqldf("select *,
                            wa / (select wa
                                from wa i
                                where i.method = o.method
                                    and i.skew = 0)
                        as wafact
                        from wa o
                        ")
head(wanorm)
# palette_colors=brewer.pal(n = 4, name = "Paired")
palette_colors = c("black", "gray20", "gray40", "gray60", "gray80")
psimnorm = ggplot(sqldf("select * from wanorm where method not in ('kgreedy')"), aes(x=factor(zipf), y=wafact, color=method, group=method)) +
  geom_line() +
  geom_point() + 
  annotate("text", x=9,   y=1.25, angle=0,  label = "greedy", color = palette_colors[1]) +
  annotate("text", x=9.1, y=1.08, angle=-10,  label = "2R-G", color = palette_colors[2]) +
  annotate("text", x=8, y=0.9,  angle=-25, label = "2R-F", color = palette_colors[3]) +
  annotate("text", x=9,   y=0.5,  angle=-40, label = "Opt", color = palette_colors[4]) +
  annotate("label", label = "simulator", x = 2.25, y = 0.35,  
           fill = "white", color = "black", label.size = NA) +  # No border
  scale_x_discrete(breaks=c(0, 0.2, 0.4, 0.6, 0.8, 1), labels=c('uniform', '0.2', '0.4', '0.6', '0.8', '1')) +
  scale_y_continuous(limits=c(0.32, 1.42), breaks=seq(0.4, 1.4, 0.2)) +
  scale_color_manual(values = palette_colors) +
  xlab("Zipf factor") +
  ylab("Relative WAF") +
  theme_bw() +
  theme(axis.text.y = element_blank()) +
  theme(legend.position = 'none', legend.margin = margin(t = -0, unit = 'cm'), axis.title.y=element_blank())
psimnorm
CairoPDF("zone-sim-zipf-norm", width=width3, height=height3, bg = "transparent")
psimnorm
dev.off()


# ###################################################################################################

# PAPER: sim vs approx plot

# Read and prepare data
op = read.csv("simop.csv")
opm = melt(op, id.vars=c('fillfactor'))
opm$fillfactor = opm$fillfactor
opm$op = 1 - opm$fillfactor
head(opm)

# Create the plot
opsim = ggplot(opm, aes(x = op, y = value, color = variable)) +
  geom_line() +
  annotate("text", label = "simulator\nGreedy", x = 0.425, y = 2.25, color = "black") +  # Keep "Sim Greedy" in original color
  annotate("text", label = "approximation\n1/(2a)", x = 0.2, y = 0.9, hjust=0.8, color = "gray40") +  # Change "approx" text label to black
  scale_color_manual(values = c(greedy = "black", approx = "gray40")) +  # "Sim Greedy" stays in original color, "approx" is black
  scale_x_continuous("OP", trans = "reverse", limits = c(0.5, 0.15), labels = scales::percent) +
  scale_y_continuous("WAF", limits = c(0, 3.2)) +  # Set y-axis limits
  theme_bw() +
  theme(legend.position = 'none')
# Save the plot to a PDF
widthInline = 2.5
heightInline = 2
CairoPDF("op-sim", width = widthInline, height = heightInline, bg = "transparent")
print(opsim)
dev.off()
