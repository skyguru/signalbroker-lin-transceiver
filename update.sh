$RIB_ID = "constexpr uint8_t rib_id = $1;"
$BEFORE = "constexpr uint8_t rib_id = 3;"

sed -e "s/$BEFORE/$RIB_ID/g"
