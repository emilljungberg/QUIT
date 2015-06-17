#!/bin/bash -e

#
# Example Processing Script For MCDESPOT data
# By Tobias Wood, with help from Anna Coombes
#

if [ $# -neq 4 ]; then
echo << END_USAGE
Usage: $0 spgr_file.nii irspgr_file.nii ssfp_file.nii mask.nii

This script will produce T1, T2 and MWF maps from DESPOT data using
DESPOT1-HIFI for B1 correction. It requires as input the SPGR, IR-SPGR and SSFP
file names, and a mask generated by FSL BET or other means.

Requires FSL.

You MUST edit the script to the flip-angles, TRs etc. used in your scans.

This script assumes that the two SSFP phase-cycling patterns have been
concatenated into a single file, e.g. with fslmerge. Pay attention to which
order this is done in.

By default this script uses all available cores on a machine. If you wish to
specify the number of cores/threads to use uncomment the NTHREADS variable.
END_USAGE
exit 1;
fi

SPGR="$1"
IRSPGR="$2"
SSFP="$3"
MASK="$4"

export QUIT_EXT=NIFTI
export FSLOUTPUTTYPE=NIFTI

#NTHREADS="-T4"

#
# EDIT THE VARIABLES IN THIS SECTION FOR YOUR SCAN PARAMETERS
#
# These values are for example purposes and likely won't work
# with your data.
#
# All times (e.g. TR) are specified in SECONDS, not milliseconds
#

SPGR_FLIP="2 3 4 5 6 7 9 13 18"
SPGR_TR="0.008"

IR_SPGR_FLIP="5"
IR_SPGR_TR="0.008"
IR_SPGR_NPE="88"
IR_SPGR_TI="0.45"

SSFP_FLIP="12.3529413 16.4705884 21.6176471 27.7941174 33.9705877 41.1764703 52.5 70"
SSFP_TR="0.003888"
SSFP_PHASE="0 180"

#
# Process DESPOT1-HIFI to get a T1, PD and B1 map
#

echo "Processing HIFI."
qidespot1hifi -n -v -m $MASK $SPGR $IRSPGR <<END_HIFI
$SPGR_FLIP
$SPGR_TR
$IR_SPGR_FLIP
$IR_SPGR_TR
$IR_SPGR_NPE
$IR_SPGR_TI
END_HIFI

# Process DESPOT2-FM to get a T2, second PD and f0 map

echo "Processing FM"
qidespot2fm -n -v -m $MASK -b HIFI_B1.nii HIFI_T1.nii $SSFP <<END_FM
$SSFP_FLIP
$SSFP_PHASE
$SSFP_TR
END_FM

# Now divide the SPGR and SSFP by their respective PD values to remove a parameter for the mcdespot fitting

fslmaths $SPGR -div D1_PD ${SPGR%%.nii*}_pd
fslmaths $SSFP -div D2_PD ${SSFP%%.nii*}_pd

# Now process MCDESPOT, using the above files, B1 and f0 maps to remove as many parameters as possible.
# Note the -S1 option which specifies scaled data

qimcdespot -n -v -m $MASK --3 -f FM_f0.nii -b HIFI_B1.nii -S1 <<END_MCD
SPGR
${SPGR%%.nii*}_pd.nii
$SPGR_FLIP
$SPGR_TR
SSFP
${SSFP%%.nii*}_pd.nii
$SSFP_FLIP
$SSFP_PHASE
$SSFP_TR
END
END_MCD
