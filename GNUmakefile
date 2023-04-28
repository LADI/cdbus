.PHONY: default
default:
	@echo "This is a project that uses the WAF build system."
	@echo ""
	@echo "A typical command sequence for configuring,"
	@echo "building and installing the software is:"
	@echo ""
	@echo "  ./waf configure --prefix=${HOME}/opt/"
	@echo "  ./waf"
	@echo "  ./waf install"
	@echo ""
	@echo "To get more info about waf configure parameters, run:"
	@echo ""
	@echo "  ./waf configure --help"
	@echo ""
	@echo "Read the README.adoc file for more info."
	@echo ""


.PHONY: README.html
README.html:
	asciidoc -b html5 -a data-uri -a icons --theme ladi -o README.html README.adoc

.PHONY: AUTHORS.regenerate
AUTHORS.regenerate:
	git shortlog -sn -- wscript . | sed -E 's@^\s+\S+\s+(.+)@\1@' > AUTHORS
	cat AUTHORS.tail >> AUTHORS
