#!/bin/bash
#
## TODO: Git tests

NC="\033[0m" # No Color
GREEN="\033[0;32m"
test_var=1
tmp=/tmp/ds_commands_tests
q=/dev/null
shell="$(ps -ef | awk '$2==pid {print $8}' pid=$$ | awk -F'/' '{ print $NF }')"
cmds="support/commands"
test_cmds="tests/data/commands"
png='assets/gcv_ex.png'
jnf1="tests/data/infer_join_fields_test1.csv"
jnf2="tests/data/infer_join_fields_test2.csv"
jnf3="tests/data/infer_join_fields_test3.scsv"
jnd1="tests/data/infer_jf_test_joined.csv"
jnd2="tests/data/infer_jf_joined_fit"
jnd3="tests/data/infer_jf_joined_fit_dz"
jnd4="tests/data/infer_jf_joined_fit_sn"
jnd5="tests/data/infer_jf_joined_fit_d2"
jnr1="tests/data/jn_repeats1"
jnr2="tests/data/jn_repeats2"
jnr3="tests/data/jn_repeats3"
jnr4="tests/data/jn_repeats4"
jnrjn1="tests/data/jn_repeats_jnd1"
jnrjn2="tests/data/jn_repeats_jnd2"
seps_base="tests/data/seps_test_base"
seps_sorted="tests/data/seps_test_sorted"
simple_csv="tests/data/company_funding_data.csv"
simple_csv2="tests/data/testcrimedata.csv"
complex_csv1="tests/data/addresses.csv"
complex_csv3="tests/data/addresses_reordered"
complex_csv2="tests/data/Sample100.csv"
complex_csv4="tests/data/quoted_fields_with_newline.csv"
complex_csv5="tests/data/taxables.csv"
complex_csv6="tests/data/quoted_multiline_fields.csv"
complex_csv6_prefield="tests/data/quoted_multiline_fields_prefield"
ls_sq="tests/data/ls_sq"
floats="tests/data/floats_test"
inferfs_chunks="tests/data/inferfs_chunks_test"
emoji="tests/data/emoji"
emojifit="tests/data/emojifit"
emoji_fit_gridlines="tests/data/emoji_fit_gridlines"
commands_fit_gridlines="tests/data/commands_shrink_fit_gridlines"
number_comma_format="tests/data/number_comma_format"

if [[ $shell =~ 'bash' ]]; then
    bsh=0
    cd "${BASH_SOURCE%/*}/.."
elif [[ $shell =~ 'zsh' ]]; then
    cd "$(dirname $0)/.."
else
    echo 'unhandled shell detected - only zsh/bash supported at this time'
    exit 1
fi

ds:fail() {
  echo "$1" && exit 1
}

make -f makefile

# BASICS TESTS

# echo -n "Running basic commands tests..."
#
# [[ $(ds:sh | grep -c "") = 1 && $(ds:sh) =~ sh ]]    || ds:fail 'sh command failed'
#
# ch="@@@COMMAND@@@ALIAS@@@DESCRIPTION@@@USAGE"
# ds:commands "" "" 0 > $q
# cmp --silent $cmds $test_cmds && grep -q "$ch" $cmds || ds:fail 'commands listing failed'
#
# ds:file_check "$png" f t &> /dev/null                || ds:fail 'file check disallowed binary files in allowed case'
#
#
# echo -e "${GREEN}PASS${NC}"

# ASSORTED COMMANDS TESTS

echo -n "Running assorted commands tests..."

actual="$(echo -e "5\n2\n4\n3\n1" | build/ds index)"
expected='1 5
2 2
3 4
4 3
5 1'
[ "$actual" = "$expected" ] || ds:fail 'index failed'

expected="abceett were ere"
actual1="$(echo -e "$expected" | build/ds random | awk '{print length($0)}')"
actual2="$(echo -e "$expected" | build/ds random tex | awk '{print length($0)}')"
expected="$(echo -e "$expected" | awk '{print length($0)}')"
[ "$actual1" = "$expected" ] || ds:fail 'random text failed noarg'
[ "$actual2" = "$expected" ] || ds:fail 'random text failed witharg'

float_re='^0.[0-9]{6}$'
actual1="$(build/ds random)"
actual2="$(build/ds random n)"
[[ "$actual1" =~ $float_re ]] || ds:fail 'random num failed noarg'
[[ "$actual2" =~ $float_re ]] || ds:fail 'random num failed witharg'
[ ! "$actual1" = "$actual2" ]      || ds:fail 'random num failed'

echo -e "${GREEN}PASS${NC}"

# CLEANUP

rm $tmp
