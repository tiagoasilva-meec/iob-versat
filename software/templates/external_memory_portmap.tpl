#{for ext external}
   #{set i index}
   #{if ext.type}
// DP
   #{for port 2}
.ext_dp_addr_@{i}_port_@{port}(ext_dp_addr_@{i}_port_@{port}),
.ext_dp_out_@{i}_port_@{port}(ext_dp_out_@{i}_port_@{port}),
.ext_dp_in_@{i}_port_@{port}(ext_dp_in_@{i}_port_@{port}),
.ext_dp_enable_@{i}_port_@{port}(ext_dp_enable_@{i}_port_@{port}),
.ext_dp_write_@{i}_port_@{port}(ext_dp_write_@{i}_port_@{port}),
   #{end}
   #{else}
// 2P
.ext_2p_addr_out_@{i}(ext_2p_addr_out_@{i}),
.ext_2p_addr_in_@{i}(ext_2p_addr_in_@{i}),
.ext_2p_write_@{i}(ext_2p_write_@{i}),
.ext_2p_read_@{i}(ext_2p_read_@{i}),
.ext_2p_data_in_@{i}(ext_2p_data_in_@{i}),
.ext_2p_data_out_@{i}(ext_2p_data_out_@{i}),
   #{end}
#{end}
