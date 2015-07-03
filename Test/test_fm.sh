#!/bin/bash -e

# Tobias Wood 2015
# Simple test script for FM, it runs slowly so test
# separately to other single-component programs.

# Tests whether programs run successfully on toy data

source ./test_common.sh
SILENCE_TESTS="1"

DATADIR="fm"
rm -rf $DATADIR
mkdir -p $DATADIR
cd $DATADIR

# First, create input data
DIMS="11 20 21"
VOXDIMS="2 2 2"
$QUITDIR/qinewimage PD.nii -d "$DIMS" -v "$VOXDIMS" -g "0 0.8 1.2"
$QUITDIR/qinewimage T1.nii -d "$DIMS" -v "$VOXDIMS" -f 1.0
$QUITDIR/qinewimage T2.nii -d "$DIMS" -v "$VOXDIMS" -g "1 0.05 1.0"
$QUITDIR/qinewimage f0.nii -d "$DIMS" -v "$VOXDIMS" -g "2 -250.0 250.0"
$QUITDIR/qinewimage B1.nii -d "$DIMS" -v "$VOXDIMS" -f 1.0

# Setup parameters
SPGR_FILE="spgr.nii"
SPGR_FLIP="5 10 15 #Test comment"
#Test line below has trailing whitespace
SPGR_TR="0.01 "
SPGR_Trf="0.002"
SPGR_TE="0.004"
SSFP_FILE="ssfp.nii"
SSFP_FLIP="15 45 70"
SSFP_TR="0.005"
SSFP_Trf="0.0025"

function run_tests() {
PREFIX="$1"
SSFP_PC="$2"

run_test "CREATE_SIGNALS" $QUITDIR/qisignal --1 -n -v -x --noise 0.005 << END_SIG
PD.nii
T1.nii
T2.nii
f0.nii
B1.nii
SPGR
$SPGR_FLIP
$SPGR_TR
x$SPGR_FILE
SSFP
$SSFP_FLIP
$SSFP_PC
$SSFP_TR
${PREFIX}x$SSFP_FILE
SPGRFinite
$SPGR_FLIP
$SPGR_TR
$SPGR_Trf
$SPGR_TE
xF${SPGR_FILE}
SSFPFinite
$SSFP_FLIP
$SSFP_PC
$SSFP_TR
$SSFP_Trf
${PREFIX}xF${SSFP_FILE}
END
END_SIG

$QUITDIR/qicomplex -x x$SPGR_FILE -M $SPGR_FILE
$QUITDIR/qicomplex -x ${PREFIX}x$SSFP_FILE -M ${PREFIX}$SSFP_FILE -P ${PREFIX}p$SSFP_FILE
$QUITDIR/qicomplex -x xF$SPGR_FILE -M F$SPGR_FILE
$QUITDIR/qicomplex -x ${PREFIX}xF$SSFP_FILE -M ${PREFIX}F$SSFP_FILE -P ${PREFIX}pF$SSFP_FILE

echo "$SSFP_FLIP
$SSFP_PC
$SSFP_TR" > ${PREFIX}fm_in.txt

echo "$SSFP_FLIP
$SSFP_PC
$SSFP_TR
$SSFP_Trf" > ${PREFIX}fm_f_in.txt

run_test "FM"    $QUITDIR/qidespot2fm -n -v -bB1.nii T1.nii ${PREFIX}$SSFP_FILE    -o ${PREFIX}       < ${PREFIX}fm_in.txt
#run_test "SRC"   $QUITDIR/qidespot2fm -n -v -bB1.nii T1.nii ${PREFIX}$SSFP_FILE    -as -o${PREFIX}s   < ${PREFIX}fm_in.txt
run_test "XFM"   $QUITDIR/qidespot2fm -n -v -bB1.nii T1.nii ${PREFIX}x$SSFP_FILE   -ax -o${PREFIX}x   < ${PREFIX}fm_in.txt
run_test "FM_F"  $QUITDIR/qidespot2fm -n -v -bB1.nii T1.nii ${PREFIX}F${SSFP_FILE} --finite -o${PREFIX}F       < ${PREFIX}fm_f_in.txt
#run_test "SRC_F" $QUITDIR/qidespot2fm -n -v -bB1.nii T1.nii ${PREFIX}F${SSFP_FILE} -as --finite -o${PREFIX}sF   < ${PREFIX}fm_f_in.txt
run_test "XFM_F" $QUITDIR/qidespot2fm -n -v -bB1.nii T1.nii ${PREFIX}xF${SSFP_FILE} --finite -ax -o${PREFIX}xF < ${PREFIX}fm_f_in.txt

compare_test "FM"    T2.nii ${PREFIX}FM_T2.nii   0.05
#compare_test "SRC"   T2.nii ${PREFIX}sFM_T2.nii  0.05
compare_test "XFM"   T2.nii ${PREFIX}xFM_T2.nii  0.05
compare_test "FM_F"  T2.nii ${PREFIX}FFM_T2.nii  0.05
#compare_test "SRC_F" T2.nii ${PREFIX}sFFM_T2.nii 0.05
compare_test "XFM_F" T2.nii ${PREFIX}xFFM_T2.nii 0.05
}

run_tests "2" "0 180"
run_tests "4" "0 90 180 270"

cd ..
SILENCE_TESTS="0"