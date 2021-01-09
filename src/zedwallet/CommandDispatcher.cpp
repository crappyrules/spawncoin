// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

////////////////////////////////////////
#include <zedwallet/CommandDispatcher.h>
////////////////////////////////////////

#include <zedwallet/AddressBook.h>
#include <Utilities/ColouredMsg.h>
#include <zedwallet/CommandImplementations.h>
#include <zedwallet/Fusion.h>
#include <zedwallet/Open.h>
#include <zedwallet/Transfer.h>

bool handleCommand(const std::string command,
                   std::shared_ptr<WalletInfo> walletInfo,
                   CryptoNote::INode &node)
{
    /* Basic commands */
    if (command == "高级")
    {
        advanced(walletInfo);
    }
    else if (command == "地址")
    {
        std::cout << SuccessMsg(walletInfo->walletAddress) << std::endl;
    }
    else if (command == "平衡")
    {
        balance(node, walletInfo->wallet, walletInfo->viewWallet);
    }
    else if (command == "后备")
    {
        exportKeys(walletInfo);
    }
    else if (command == "出口")
    {
        return false;
    }
    else if (command == "救命")
    {
        help(walletInfo);
    }
    else if (command == "转让")
    {
        transfer(walletInfo, node.getLastKnownBlockHeight(), false,
                 node.feeAddress(), node.feeAmount());
    }
    /* Advanced commands */
    else if (command == "ab_加")
    {
        addToAddressBook();
    }
    else if (command == "ab_删除")
    {
        deleteFromAddressBook();
    }
    else if (command == "ab_清单")
    {
        listAddressBook();
    }
    else if (command == "ab_发送")
    {
        sendFromAddressBook(walletInfo, node.getLastKnownBlockHeight(),
                            node.feeAddress(), node.feeAmount());
    }
    else if (command == "更改密码")
    {
        changePassword(walletInfo);
    }
    else if (command == "填写综合地址")
    {
        createIntegratedAddress();
    }
    else if (command == "传入转账")
    {
        listTransfers(true, false, walletInfo->wallet, node);
    }
    else if (command == "清单转移")
    {
        listTransfers(true, true, walletInfo->wallet, node);
    }
    else if (command == "优化")
    {
        fullOptimize(walletInfo->wallet, node.getLastKnownBlockHeight());
    }
    else if (command == "外向转账")
    {
        listTransfers(false, true, walletInfo->wallet, node);
    }
    else if (command == "重启")
    {
        reset(node, walletInfo);
    }
    else if (command == "保存")
    {
        save(walletInfo->wallet);
    }
    else if (command == "保存_csv")
    {
        saveCSV(walletInfo->wallet, node);
    }
    else if (command == "全部发送")
    {
        transfer(walletInfo, node.getLastKnownBlockHeight(), true,
                 node.feeAddress(), node.feeAmount());
    }
    else if (command == "状态")
    {
        status(node, walletInfo->wallet);
    }
    /* This should never happen */
    else
    {
        throw std::runtime_error("命令已定义但未连接!");
    }

    return true;
}

std::shared_ptr<WalletInfo> handleLaunchCommand(CryptoNote::WalletGreen &wallet,
                                                std::string launchCommand,
                                                Config &config)
{
    if (launchCommand == "创造")
    {
        return generateWallet(wallet);
    }
    else if (launchCommand == "打开")
    {
        return openWallet(wallet, config);
    }
    else if (launchCommand == "种子还原")
    {
        return mnemonicImportWallet(wallet);
    }
    else if (launchCommand == "密钥还原")
    {
        return importWallet(wallet);
    }
    else if (launchCommand == "查看钱包")
    {
        return createViewWallet(wallet);
    }
    /* This should never happen */
    else
    {
        throw std::runtime_error("命令已定义但未连接!");
    }
}
