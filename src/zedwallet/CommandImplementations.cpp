// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

/////////////////////////////////////////////
#include <zedwallet/CommandImplementations.h>
/////////////////////////////////////////////

#include <atomic>

#include <Common/StringTools.h>

#include <config/WalletConfig.h>

#include <CryptoNoteCore/Account.h>
#include <Common/TransactionExtra.h>

#ifndef MSVC
#include <fstream>
#endif

#include <Mnemonics/Mnemonics.h>

#include <Utilities/FormatTools.h>

#include <zedwallet/AddressBook.h>
#include <Utilities/ColouredMsg.h>
#include <zedwallet/Commands.h>
#include <zedwallet/Fusion.h>
#include <zedwallet/Menu.h>
#include <zedwallet/Open.h>
#include <zedwallet/Sync.h>
#include <zedwallet/Tools.h>
#include <zedwallet/Transfer.h>
#include <zedwallet/Types.h>

void changePassword(std::shared_ptr<WalletInfo> walletInfo)
{
    /* Check the user knows the current password */
    confirmPassword(walletInfo->walletPass, "确认您的当前密码: ");

    /* Get a new password for the wallet */
    const std::string newPassword
        = getWalletPassword(true, "输入新密码: ");

    /* Change the wallet password */
    walletInfo->wallet.changePassword(walletInfo->walletPass, newPassword);

    /* Change the stored wallet metadata */
    walletInfo->walletPass = newPassword;

    /* Make sure we save with the new password */
    walletInfo->wallet.save();

    std::cout << SuccessMsg("您的密码已被更改!") << std::endl;
}

void exportKeys(std::shared_ptr<WalletInfo> walletInfo)
{
    confirmPassword(walletInfo->walletPass);
    printPrivateKeys(walletInfo->wallet, walletInfo->viewWallet);
}

void printPrivateKeys(CryptoNote::WalletGreen &wallet, bool viewWallet)
{
    const Crypto::SecretKey privateViewKey = wallet.getViewKey().secretKey;

    if (viewWallet)
    {
        std::cout << SuccessMsg("私密视图键:")
                  << std::endl
                  << SuccessMsg(Common::podToHex(privateViewKey))
                  << std::endl;
        return;
    }

    Crypto::SecretKey privateSpendKey = wallet.getAddressSpendKey(0).secretKey;

    Crypto::SecretKey derivedPrivateViewKey;

    Crypto::crypto_ops::generateViewFromSpend(privateSpendKey,
                                                   derivedPrivateViewKey);

    const bool deterministicPrivateKeys
             = derivedPrivateViewKey == privateViewKey;

    std::cout << SuccessMsg("私人消费钥匙:")
              << std::endl
              << SuccessMsg(Common::podToHex(privateSpendKey))
              << std::endl
              << std::endl
              << SuccessMsg("私密视图键:")
              << std::endl
              << SuccessMsg(Common::podToHex(privateViewKey))
              << std::endl;

    if (deterministicPrivateKeys)
    {
        std::cout << std::endl
                  << SuccessMsg("助记种子:")
                  << std::endl
                  << SuccessMsg(Mnemonics::PrivateKeyToMnemonic(privateSpendKey))
                  << std::endl;
    }
}

void balance(CryptoNote::INode &node, CryptoNote::WalletGreen &wallet,
             bool viewWallet)
{
    const uint64_t unconfirmedBalance = wallet.getPendingBalance();
    uint64_t confirmedBalance = wallet.getActualBalance();

    const uint32_t localHeight = node.getLastLocalBlockHeight();
    const uint32_t remoteHeight = node.getLastKnownBlockHeight();
    const uint32_t walletHeight = wallet.getBlockCount();

    /* We can make a better approximation of the view wallet balance if we
       ignore fusion transactions.
       See https://github.com/turtlecoin/turtlecoin/issues/531 */
    if (viewWallet)
    {
        /* Not sure how to verify if a transaction is unlocked or not via
           the WalletTransaction type, so this is technically not correct,
           we might be including locked balance. */
        confirmedBalance = 0;

        size_t numTransactions = wallet.getTransactionCount();

        for (size_t i = 0; i < numTransactions; i++)
        {
            const CryptoNote::WalletTransaction t = wallet.getTransaction(i);

            /* Fusion transactions are zero fee, skip them. Coinbase
               transactions are also zero fee, include them. */
            if (t.fee != 0 || t.isBase)
            {
                confirmedBalance += t.totalAmount;
            }
        }
    }

    const uint64_t totalBalance = unconfirmedBalance + confirmedBalance;

    std::cout << "可用余额: "
              << SuccessMsg(formatAmount(confirmedBalance)) << std::endl
              << "锁定（未确认）余额: "
              << WarningMsg(formatAmount(unconfirmedBalance))
              << std::endl << "总余额: "
              << InformationMsg(formatAmount(totalBalance)) << std::endl;

    if (viewWallet)
    {
        std::cout << std::endl
                  << InformationMsg("请注意，仅查看钱包 "
                                    "只能跟踪传入的交易,")
                  << std::endl
                  << InformationMsg("因此您的钱包余额可能会出现 "
                                    "膨胀的.") << std::endl;
    }

    if (localHeight < remoteHeight)
    {
        std::cout << std::endl
                  << InformationMsg("您的守护程序未与网络完全同步!")
                  << std::endl
                  << "Your balance may be incorrect until you are fully "
                  << "synced!" << std::endl;
    }
    /* Small buffer because wallet height doesn't update instantly like node
       height does */
    else if (walletHeight + 1000 < remoteHeight)
    {
        std::cout << std::endl
                  << InformationMsg("区块链仍在进行交易扫描.")
                  << std::endl
                  << "进行中时余额可能不正确."
                  << std::endl;
    }
}

void printHeights(uint32_t localHeight, uint32_t remoteHeight,
                  uint32_t walletHeight)
{
    /* This is the height that the wallet has been scanned to. The blockchain
       can be fully updated, but we have to walk the chain to find our
       transactions, and this number indicates that progress. */
    std::cout << "钱包区块链高度: ";

    /* Small buffer because wallet height doesn't update instantly like node
       height does */
    if (walletHeight + 1000 > remoteHeight)
    {
        std::cout << SuccessMsg(std::to_string(walletHeight));
    }
    else
    {
        std::cout << WarningMsg(std::to_string(walletHeight));
    }

    std::cout << std::endl << "本地区块链高度: ";

    if (localHeight == remoteHeight)
    {
        std::cout << SuccessMsg(std::to_string(localHeight));
    }
    else
    {
        std::cout << WarningMsg(std::to_string(localHeight));
    }

    std::cout << std::endl << "网络区块链高度: "
              << SuccessMsg(std::to_string(remoteHeight)) << std::endl;
}

void printSyncStatus(uint32_t localHeight, uint32_t remoteHeight,
                     uint32_t walletHeight)
{
    std::string networkSyncPercentage
        = Utilities::get_sync_percentage(localHeight, remoteHeight) + "%";

    std::string walletSyncPercentage
        = Utilities::get_sync_percentage(walletHeight, remoteHeight) + "%";

    std::cout << "网络同步状态: ";

    if (localHeight == remoteHeight)
    {
        std::cout << SuccessMsg(networkSyncPercentage) << std::endl;
    }
    else
    {
        std::cout << WarningMsg(networkSyncPercentage) << std::endl;
    }

    std::cout << "钱包同步状态: ";

    /* Small buffer because wallet height is not always completely accurate */
    if (walletHeight + 1000 > remoteHeight)
    {
        std::cout << SuccessMsg(walletSyncPercentage) << std::endl;
    }
    else
    {
        std::cout << WarningMsg(walletSyncPercentage) << std::endl;
    }
}

void printSyncSummary(uint32_t localHeight, uint32_t remoteHeight,
                      uint32_t walletHeight)
{
    if (localHeight == 0 && remoteHeight == 0)
    {
        std::cout << WarningMsg("嗯，看来你没有 ")
                  << WarningMsg(WalletConfig::daemonName)
                  << WarningMsg(" 打开!")
                  << std::endl;
    }
    else if (walletHeight + 1000 < remoteHeight && localHeight == remoteHeight)
    {
        std::cout << InformationMsg("您已与网络同步，但仍在扫描区块链中的交易.")
                  << std::endl
                  << "进行中时余额可能不正确."
                  << std::endl;
    }
    else if (localHeight == remoteHeight)
    {
        std::cout << SuccessMsg("好极了! 您已同步!") << std::endl;
    }
    else
    {
        std::cout << WarningMsg("请耐心等待，您仍在与网络同步!") << std::endl;
    }
}

void printPeerCount(size_t peerCount)
{
    std::cout << "同行: " << SuccessMsg(std::to_string(peerCount))
              << std::endl;
}

void printHashrate(uint64_t difficulty)
{
    /* Offline node / not responding */
    if (difficulty == 0)
    {
        return;
    }

    /* Hashrate is difficulty divided by block target time */
    uint32_t hashrate = static_cast<uint32_t>(
        round(difficulty / CryptoNote::parameters::DIFFICULTY_TARGET)
    );

    std::cout << "网络哈希率: "
              << SuccessMsg(Utilities::get_mining_speed(hashrate))
              << " (基于最后一个本地块)" << std::endl;
}

/* This makes sure to call functions on the node which only return cached
   data. This ensures it returns promptly, and doesn't hang waiting for a
   response when the node is having issues. */
void status(CryptoNote::INode &node, CryptoNote::WalletGreen &wallet)
{
    uint32_t localHeight = node.getLastLocalBlockHeight();
    uint32_t remoteHeight = node.getLastKnownBlockHeight();
    uint32_t walletHeight = wallet.getBlockCount();

    /* Print the heights of local, remote, and wallet */
    printHeights(localHeight, remoteHeight, walletHeight);

    std::cout << std::endl;

    /* Print the network and wallet sync status in percentage */
    printSyncStatus(localHeight, remoteHeight, walletHeight);

    std::cout << std::endl;

    /* Print the network hashrate, based on the last local block */
    printHashrate(node.getLastLocalBlockHeaderInfo().difficulty);

    /* Print the amount of peers we have */
    printPeerCount(node.getPeerCount());

    std::cout << std::endl;

    /* Print a summary of the sync status */
    printSyncSummary(localHeight, remoteHeight, walletHeight);
}

void reset(CryptoNote::INode &node, std::shared_ptr<WalletInfo> walletInfo)
{
    uint64_t scanHeight = getScanHeight();

    std::cout << std::endl
              << InformationMsg("此过程可能需要一些时间才能完成.")
              << std::endl
              << InformationMsg("您在交易期间无法进行任何交易 ")
              << InformationMsg("处理.")
              << std::endl << std::endl;

    if (!confirm("你确定吗?"))
    {
        return;
    }

    std::cout << InformationMsg("重置钱包...") << std::endl;

    walletInfo->wallet.reset(scanHeight);

    syncWallet(node, walletInfo);
}

void saveCSV(CryptoNote::WalletGreen &wallet, CryptoNote::INode &node)
{
    const size_t numTransactions = wallet.getTransactionCount();

    std::ofstream csv;
    csv.open(WalletConfig::csvFilename);

    if (!csv)
    {
        std::cout << WarningMsg("无法打开transactions.csv文件进行保存!")
                  << std::endl
                  << WarningMsg("确保它没有在任何其他应用程序中打开.")
                  << std::endl;
        return;
    }

    std::cout << InformationMsg("保存CSV文件...") << std::endl;

    /* Create CSV header */
    csv << "时间戳,块高,哈希,金额,输入/输出"
        << std::endl;

    /* Loop through transactions */
    for (size_t i = 0; i < numTransactions; i++)
    {
        const CryptoNote::WalletTransaction t = wallet.getTransaction(i);

        /* Ignore fusion transactions */
        if (t.totalAmount == 0)
        {
            continue;
        }

        const std::string amount = formatAmountBasic(std::abs(t.totalAmount));

        const std::string direction = t.totalAmount > 0 ? "IN" : "OUT";

        csv << unixTimeToDate(t.timestamp) << ","       /* Timestamp */
            << t.blockHeight << ","                     /* Block Height */
            << Common::podToHex(t.hash) << ","          /* Hash */
            << amount << ","                            /* Amount */
            << direction                                /* In/Out */
            << std::endl;
    }

    csv.close();

    std::cout << SuccessMsg("CSV已成功写入 ")
              << SuccessMsg(WalletConfig::csvFilename)
              << SuccessMsg("!")
              << std::endl;
}

void printOutgoingTransfer(CryptoNote::WalletTransaction t,
                           CryptoNote::INode &node)
{
    std::cout << WarningMsg("外向转移:")
              << std::endl
              << WarningMsg("杂凑: " + Common::podToHex(t.hash))
              << std::endl;

    if (t.timestamp != 0)
    {
        std::cout << WarningMsg("块高: ")
                  << WarningMsg(std::to_string(t.blockHeight))
                  << std::endl
                  << WarningMsg("时间戳记: ")
                  << WarningMsg(unixTimeToDate(t.timestamp))
                  << std::endl;
    }

    std::cout << WarningMsg("花费: " + formatAmount(-t.totalAmount - t.fee))
              << std::endl
              << WarningMsg("费用: " + formatAmount(t.fee))
              << std::endl
              << WarningMsg("总花费: " + formatAmount(-t.totalAmount))
              << std::endl;

    const std::string paymentID = getPaymentIDFromExtra(t.extra);

    if (paymentID != "")
    {
        std::cout << WarningMsg("付款编号: " + paymentID) << std::endl;
    }

    std::cout << std::endl;
}

void printIncomingTransfer(CryptoNote::WalletTransaction t,
                           CryptoNote::INode &node)
{
    std::cout << SuccessMsg("传入转帐:")
              << std::endl
              << SuccessMsg("杂凑: " + Common::podToHex(t.hash))
              << std::endl;

    if (t.timestamp != 0)
    {
        std::cout << SuccessMsg("块高: ")
                  << SuccessMsg(std::to_string(t.blockHeight))
                  << std::endl
                  << SuccessMsg("时间戳记: ")
                  << SuccessMsg(unixTimeToDate(t.timestamp))
                  << std::endl;
    }


    std::cout << SuccessMsg("量: " + formatAmount(t.totalAmount))
              << std::endl;

    const std::string paymentID = getPaymentIDFromExtra(t.extra);

    if (paymentID != "")
    {
        std::cout << SuccessMsg("付款编号: " + paymentID) << std::endl;
    }

    std::cout << std::endl;
}

void listTransfers(bool incoming, bool outgoing,
                   CryptoNote::WalletGreen &wallet, CryptoNote::INode &node)
{
    const size_t numTransactions = wallet.getTransactionCount();

    int64_t totalSpent = 0;
    int64_t totalReceived = 0;

    for (size_t i = 0; i < numTransactions; i++)
    {
        const CryptoNote::WalletTransaction t = wallet.getTransaction(i);

        /* Is a fusion transaction (on a view only wallet). It appears to have
           an incoming amount, because we can't detract the outputs (can't
           decrypt them) */
        if (t.fee == 0 && !t.isBase)
        {
            continue;
        }

        if (t.totalAmount < 0 && outgoing)
        {
            printOutgoingTransfer(t, node);
            totalSpent += -t.totalAmount;
        }
        else if (t.totalAmount > 0 && incoming)
        {
            printIncomingTransfer(t, node);
            totalReceived += t.totalAmount;
        }
    }

    if (incoming)
    {
        std::cout << SuccessMsg("收到的总数: "
                              + formatAmount(totalReceived))
                  << std::endl;
    }

    if (outgoing)
    {
        std::cout << WarningMsg("总支出: " + formatAmount(totalSpent))
                  << std::endl;
    }
}

void save(CryptoNote::WalletGreen &wallet)
{
    std::cout << InformationMsg("保存.") << std::endl;
    wallet.save();
    std::cout << InformationMsg("已保存.") << std::endl;
}

void createIntegratedAddress()
{
    std::cout << InformationMsg("根据地址和付款ID对创建集成地址...")
              << std::endl << std::endl;

    std::string address;
    std::string paymentID;

    while (true)
    {
        std::cout << InformationMsg("地址: ");

        std::getline(std::cin, address);
        trim(address);

        std::cout << std::endl;

        if (parseStandardAddress(address, true))
        {
            break;
        }
    }

    while (true)
    {
        std::cout << InformationMsg("付款编号: ");

        std::getline(std::cin, paymentID);
        trim(paymentID);

        std::vector<uint8_t> extra;

        std::cout << std::endl;

        if (!CryptoNote::createTxExtraWithPaymentId(paymentID, extra))
        {
            std::cout << WarningMsg("无法解析! 付款ID为64个字符的十六进制字符串.")
                      << std::endl << std::endl;

            continue;
        }

        break;
    }

    std::cout << InformationMsg(createIntegratedAddress(address, paymentID))
              << std::endl;
}

void help(std::shared_ptr<WalletInfo> wallet)
{
    if (wallet->viewWallet)
    {
        printCommands(basicViewWalletCommands());
    }
    else
    {
        printCommands(basicCommands());
    }
}

void advanced(std::shared_ptr<WalletInfo> wallet)
{
    /* We pass the offset of the command to know what index to print for
       command numbers */
    if (wallet->viewWallet)
    {
        printCommands(advancedViewWalletCommands(),
                      basicViewWalletCommands().size());
    }
    else
    {
        printCommands(advancedCommands(),
                      basicCommands().size());
    }
}
