# This file is part of MLDB. Copyright 2015 Datacratic. All rights reserved.

#------------------------------------------------------------------------------#
# testing.mk
# Rémi Attab, 26 Jul 2013
# Copyright (c) 2013 Datacratic.  All rights reserved.
#
# Makefile for the tests of soa's utilities.
#------------------------------------------------------------------------------#

$(eval $(call test,fixture_test,test_utils,boost))
$(eval $(call test,print_utils_test,,boost))
$(eval $(call test,type_traits_test,,boost))
$(eval $(call test,csv_writer_test,csv_writer,boost))
