#!/bin/sh
DEBUGFS=/sys/kernel/debug

clear

echo "Resume D1 device"

echo 1 > $DEBUGFS/fake_dev_driver_D1/rpm_get


#trace-cmd start -p function -l "fake_dev_*" -l "pm_runtime*" -l "arm_cpuidle_suspend" \
#	-l "arm_pd_power_*" \
#	-l "*genpd_power*" \

cat $DEBUGFS/pm_genpd/states

echo "Set D1 device constraint: suspend+resume time shall not be longer than 2.2s"
echo "D1 registered a matching retention state d1-retention, that can be selected"
echo "by the domain governor."

echo 0 > $DEBUGFS/fake_dev_driver_D1/constraint_us
echo 2200 > $DEBUGFS/fake_dev_driver_D1/constraint_us
echo 1 > $DEBUGFS/fake_dev_driver_D1/rpm_put
echo "'Put' D1, wait for autosuspend..."
cat $DEBUGFS/pm_genpd/summary | grep cpu1

echo
echo "try go to sleep for more than 2 sec..."
sleep 3
echo "and check if the retention state was selected:"

cat $DEBUGFS/pm_genpd/summary
cat $DEBUGFS/pm_genpd/timings

#trace-cmd stop
#trace-cmd extract
#cp /root/trace.dat /


