#ifndef ELLIPTICS_CLIENT_STATE_HPP__
#define ELLIPTICS_CLIENT_STATE_HPP__

#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"

#include <cocaine/common.hpp> // for configuration_error_t
#include <elliptics/cppdef.h>

using namespace ioremap;
using namespace cocaine;

struct elliptics_client_state {
	std::shared_ptr<elliptics::file_logger> logger;
	std::shared_ptr<elliptics::node> node;
	std::vector<int> groups;

	elliptics::session create_session() {
		elliptics::session session(*node);
		session.set_groups(groups);
		return session;
	}

	// bare default: single target server, single group, no logging (but could be enabled)
	static elliptics_client_state create(const std::string &server_addr, int server_port, int group, int loglevel = 0)
	{
		elliptics_client_state result;   
		result.logger.reset(new elliptics::file_logger("/dev/stderr", loglevel));
		result.node.reset(new elliptics::node(*result.logger));
		result.node->add_remote(server_addr.c_str(), server_port);
		result.groups.push_back(group);
		return result;
	}

	// Configure from preparsed json config.
	// Config template:
	// {
	//   remotes: ["localhost:1025:2"],
	//   groups: [2],
	//   logfile: "/dev/stderr",
	//   loglevel: 0
	// }
	static elliptics_client_state create(const rapidjson::Document &args)
	{
		std::string logfile = "/dev/stderr";
		uint loglevel = DNET_LOG_INFO;
		std::vector<std::string> remotes;
		std::vector<int> groups;

		try {
			if (args.HasMember("logfile"))
				logfile = args["logfile"].GetString();

			if (args.HasMember("loglevel"))
				loglevel = args["loglevel"].GetInt();

			const rapidjson::Value &remotesArray = args["remotes"];
			std::transform(remotesArray.Begin(), remotesArray.End(),
				std::back_inserter(remotes),
				std::bind(&rapidjson::Value::GetString, std::placeholders::_1)
				);
			const rapidjson::Value &groupsArray = args["groups"];
			std::transform(groupsArray.Begin(), groupsArray.End(),
				std::back_inserter(groups),
				std::bind(&rapidjson::Value::GetInt, std::placeholders::_1)
				);
		} catch (const std::exception &e) {
			throw configuration_error_t(e.what());
		}

		return create(remotes, groups, logfile, loglevel);
	}

	static elliptics_client_state create(const std::vector<std::string> &remotes,
			const std::vector<int> &groups, const std::string &logfile, int loglevel)
	{
		if (remotes.size() == 0) {
			throw configuration_error_t("no remotes have been specified");
		}
		if (groups.size() == 0) {
			throw configuration_error_t("no groups have been specified");
		}

		elliptics_client_state result;
		result.logger.reset(new elliptics::file_logger(logfile.c_str(), loglevel));
		result.node.reset(new elliptics::node(*result.logger));
		result.groups = groups;

		if (remotes.size() == 1) {
			// any error is fatal if there is a single remote address
			result.node->add_remote(remotes.front().c_str());

		} else {
			// add_remote throws errors if:
			//  * it can not parse address
			//  * it can not connect to a specified address
			//  * there is address duplication (NOTE: is this still true?)
			// In any case we ignore all errors in hope that at least one would suffice.
			int added = 0;
			for (const auto &i : remotes) {
				try {
					result.node->add_remote(i.c_str());
					++added;
				} catch (const elliptics::error &) {
					// pass
				}
			}
			if (added == 0) {
				throw configuration_error_t("no remotes were added successfully");
			}
		}

		return result;
	}
};

#endif // ELLIPTICS_CLIENT_STATE_HPP__
