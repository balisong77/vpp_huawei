# Copyright (c) 2022-2024, Arm Limited.
#
# SPDX-License-Identifier: Apache-2.0

.PHONY: license-check
license-check:
	@$(CURDIR)/addLicense.py --quiet \
		--includes $(CURDIR)/.. \
		--excludes $(CURDIR)/../LICENSE.rst $(CURDIR)/../components \
		$(CURDIR)/check-git-log.sh $(CURDIR)/git-log-fixes.sh \
		$(CURDIR)/../doc/venv $(CURDIR)/traffic-gen \
		|| exit $$?

.PHONY: license-fix
license-fix:
	@$(CURDIR)/addLicense.py --quiet --fix\
		--includes $(CURDIR)/.. \
		--excludes $(CURDIR)/../LICENSE.rst $(CURDIR)/../components \
		$(CURDIR)/check-git-log.sh $(CURDIR)/git-log-fixes.sh \
		$(CURDIR)/../doc/venv $(CURDIR)/traffic-gen \
		|| exit $$?

.PHONY: commitlog-check
commitlog-check:
	@$(CURDIR)/check-git-log.sh -n1 || exit $$?

.PHONY: traffic-gen
traffic-gen:
	@$(MAKE) -C traffic-gen all

.PHONY: help
help:
	@echo "Make Targets:"
	@echo " license-check        - Check whether license headers conform format"
	@echo " license-fix          - Fix the license headers with uniform format"
	@echo " commitlog-check      - Check commit message format"
	@echo " traffic-gen          - Build traffic generator tools, e.g., wrk2"
