library(duckdb)

duck_con = dbConnect(duckdb())

duck_register = function() {
	frames = rev(sys.frames())[-1]
	added = character() 
	i = 0
	for (f in frames) { # iterate over stack frames from inner to outer
		lns	= ls(envir = f)
		for (l in lns) {
			if (!(l %in% added)) { # makes sure that inner obj are not overwritten by outer ones
				obj = tryCatch(get(l, envir = f, inherits=FALSE), error = function(e) NULL)
				if (!is.null(obj) && is.data.frame(obj) && ncol(obj) > 0) { # ncol > 0, otherwise duckdb will complain
					# cat("l: "); print(l) # ; cat(" obj: ") ; # print(obj)
					duckdb_unregister(duck_con, l)
					duckdb_register(duck_con, l, obj)
				}
				added = c(added, l)
			}
		}
		i = i+1
	}
	# cat("registered: "); print(i)
}
duck = function(query) {
	duck_register()
	return(dbGetQuery(duck_con, query))
}
duckdf = function(query) {
	duck(query)
}
