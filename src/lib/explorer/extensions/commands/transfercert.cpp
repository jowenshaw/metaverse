/**
 * Copyright (c) 2016-2018 mvs developers
 *
 * This file is part of metaverse-explorer.
 *
 * metaverse-explorer is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Affero General Public License with
 * additional permissions to the one published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version. For more information see LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <metaverse/explorer/json_helper.hpp>
#include <metaverse/explorer/extensions/commands/transfercert.hpp>
#include <metaverse/explorer/extensions/command_extension_func.hpp>
#include <metaverse/explorer/extensions/command_assistant.hpp>
#include <metaverse/explorer/extensions/exception.hpp>
#include <metaverse/explorer/extensions/base_helper.hpp>

namespace libbitcoin {
namespace explorer {
namespace commands {


console_result transfercert::invoke (Json::Value& jv_output,
    libbitcoin::server::server_node& node)
{
    auto& blockchain = node.chain_impl();
    blockchain.is_account_passwd_valid(auth_.name, auth_.auth);

    blockchain.uppercase_symbol(argument_.symbol);
    blockchain.uppercase_symbol(argument_.type_name);

    // check asset symbol
    check_asset_symbol(argument_.symbol);

    // check asset cert types
    std::map <std::string, asset_cert_type> cert_param_value_map = {
        {"ISSUE", asset_cert_ns::issue},
        {"DOMAIN", asset_cert_ns::domain},
        {"NAMING", asset_cert_ns::naming}
    };

    auto iter = cert_param_value_map.find(argument_.type_name);
    if (iter == cert_param_value_map.end()) {
        throw asset_cert_exception("unsupported asset cert type " + argument_.type_name);
    }

    auto cert_type = iter->second;
    if (cert_type == asset_cert_ns::issue) {
        auto sh_asset = blockchain.get_issued_asset(argument_.symbol);
        if (!sh_asset)
            throw asset_symbol_notfound_exception(
                "asset '" + argument_.symbol + "' does not exist.");
    }

    // check cert is owned by the account
    std::shared_ptr<asset_cert> cert = blockchain.get_asset_cert(argument_.symbol, cert_type);
    if (!cert) {
        throw asset_cert_notfound_exception(
            "cert '" + argument_.symbol + "' does not exist.");
    }

    auto from_did = cert->get_owner();
    auto from_address = cert->get_address();
    if (!blockchain.get_account_address(auth_.name, from_address)) {
        throw asset_cert_notowned_exception(
            "cert '" + argument_.symbol + "' is not owned by " + auth_.name);
    }

    // check target address
    auto to_did = argument_.to;
    auto to_address = get_address_from_did(to_did, blockchain);
    if (!blockchain.is_valid_address(to_address))
        throw address_invalid_exception{"invalid did parameter! " + to_did};

    // receiver
    std::vector<receiver_record> receiver{
        {to_address, argument_.symbol, 0, 0,
            cert_type, utxo_attach_type::asset_cert, attachment(from_did, to_did)}
    };

    auto helper = transferring_asset_cert(*this, blockchain,
        std::move(auth_.name), std::move(auth_.auth),
        "", std::move(argument_.symbol),
        std::move(receiver), argument_.fee);

    helper.exec();

    // json output
    auto tx = helper.get_transaction();
    jv_output =  config::json_helper(get_api_version()).prop_tree(tx, true);

    return console_result::okay;
}


} // namespace commands
} // namespace explorer
} // namespace libbitcoin

