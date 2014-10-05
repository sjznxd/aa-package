--[[
LuCI - Lua Configuration Interface

Copyright 2014 bywayboy <bywayboy@qq.com>

]]--

module("luci.controller.phddns", package.seeall)

function index()
	
	local page
                   
	if nixio.fs.access("/etc/config/phddns") then
		page = entry({"admin", "services", "phddns"}, cbi("admin_services/phddns"), _("Oray DDNS"))
		page.dependent = true
		
		page = entry({"admin", "services", "subversion"},cbi("admin_services/subversion"), _("Subversion"))
		page.dependent = true

		page = entry({"admin", "services", "vlmcsd"},cbi("admin_services/vlmcsd"), _("KMS Server"))
		page.dependent = true
	end
end
