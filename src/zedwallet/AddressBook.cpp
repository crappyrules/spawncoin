// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

//////////////////////////////////
#include <zedwallet/AddressBook.h>
//////////////////////////////////

#ifndef MSVC
#include <fstream>
#endif

#include <Serialization/SerializationTools.h>

#include <Wallet/WalletUtils.h>

#include <Utilities/ColouredMsg.h>
#include <zedwallet/Tools.h>
#include <zedwallet/Transfer.h>
#include <config/WalletConfig.h>

const std::string getAddressBookName(AddressBook addressBook)
{
    while (true)
    {
        std::string friendlyName;

        std::cout << InformationMsg("您想要什么友好的名字 ")
                  << InformationMsg("给这个通讯录条目?: ");

        std::getline(std::cin, friendlyName);
        trim(friendlyName);

        const auto it = std::find(addressBook.begin(), addressBook.end(),
                            AddressBookEntry(friendlyName));

        if (it != addressBook.end())
        {
            std::cout << WarningMsg("与此相关的通讯录条目 ")
                      << WarningMsg("名称已经存在!")
                      << std::endl << std::endl;

            continue;
        }

        return friendlyName;
    }
}

const Maybe<std::string> getAddressBookPaymentID()
{
    std::stringstream msg;

    msg << std::endl
        << "此通讯录条目是否具有与其关联的付款ID?"
        << std::endl;

    return getPaymentID(msg.str());
}

void addToAddressBook()
{
    std::cout << InformationMsg("注意：您可以随时输入“取消”来 "
                                "取消将某人添加到您的地址簿")
              << std::endl << std::endl;

    auto addressBook = getAddressBook();

    const std::string friendlyName = getAddressBookName(addressBook);

    if (friendlyName == "取消")
    {
        std::cout << WarningMsg("取消添加到地址簿.")
                  << std::endl;
        return;
    }

    auto maybeAddress = getAddress("\n该用户有什么地址? ");

    if (!maybeAddress.isJust)
    {
        std::cout << WarningMsg("取消添加到地址簿.")
                  << std::endl;
        return;
    }

    std::string address = maybeAddress.x.second;
    std::string paymentID = "";

    bool integratedAddress = maybeAddress.x.first == IntegratedAddress;

    if (!integratedAddress)
    {
        auto maybePaymentID = getAddressBookPaymentID();

        if (!maybePaymentID.isJust)
        {
            std::cout << WarningMsg("取消添加到地址簿.")
                      << std::endl;

            return;
        }

        paymentID = maybePaymentID.x;
    }

    addressBook.emplace_back(friendlyName, address, paymentID,
                             integratedAddress);

    if (saveAddressBook(addressBook))
    {
        std::cout << std::endl
                  << SuccessMsg("新条目已添加到您的地址 ")
                  << SuccessMsg("书!")
                  << std::endl;
    }
}

const Maybe<const AddressBookEntry> getAddressBookEntry(AddressBook addressBook)
{
    while (true)
    {
        std::string friendlyName;

        std::cout << InformationMsg("您想发送给谁 ")
                  << InformationMsg("地址簿?: ");

        std::getline(std::cin, friendlyName);
        trim(friendlyName);

        if (friendlyName == "取消")
        {
            return Nothing<const AddressBookEntry>();
        }

        auto it = std::find(addressBook.begin(), addressBook.end(),
                            AddressBookEntry(friendlyName));

        if (it != addressBook.end())
        {
            return Just<const AddressBookEntry>(*it);
        }

        std::cout << std::endl
                  << WarningMsg("找不到名称为的用户 ")
                  << InformationMsg(friendlyName)
                  << WarningMsg(" 在您的通讯录中!")
                  << std::endl << std::endl;

        const bool list = confirm("您想列出您的所有人吗 "
                                  "地址簿?");

        std::cout << std::endl;

        if (list)
        {
            listAddressBook();
        }
    }
}

void sendFromAddressBook(std::shared_ptr<WalletInfo> walletInfo,
                         uint32_t height, std::string feeAddress,
                         uint32_t feeAmount)
{
    auto addressBook = getAddressBook();

    if (isAddressBookEmpty(addressBook))
    {
        return;
    }

    std::cout << InformationMsg("注意：您可以随时输入“取消”来 ")
              << InformationMsg("取消交易")
              << std::endl
              << std::endl;

    auto maybeAddressBookEntry = getAddressBookEntry(addressBook);

    if (!maybeAddressBookEntry.isJust)
    {
        std::cout << WarningMsg("取消交易.") << std::endl;
        return;
    }

    auto addressBookEntry = maybeAddressBookEntry.x;

    auto maybeAmount = getTransferAmount();

    if (!maybeAmount.isJust)
    {
        std::cout << WarningMsg("取消交易.") << std::endl;
        return;
    }

    /* Originally entered address, so we can preserve the correct integrated
       address for confirmation screen */
    auto originalAddress = addressBookEntry.address;
    auto address = originalAddress;
    auto amount = maybeAmount.x;
    auto fee = WalletConfig::defaultFee;
    auto extra = getExtraFromPaymentID(addressBookEntry.paymentID);
    auto mixin = CryptoNote::getDefaultMixinByHeight(height);
    auto integrated = addressBookEntry.integratedAddress;

    if (integrated)
    {
        auto addrPaymentIDPair = extractIntegratedAddress(address);
        address = addrPaymentIDPair.x.first;
        extra = getExtraFromPaymentID(addrPaymentIDPair.x.second);
    }

    doTransfer(address, amount, fee, extra, walletInfo, height, integrated,
               mixin, feeAddress, feeAmount, originalAddress);
}

bool isAddressBookEmpty(AddressBook addressBook)
{
    if (addressBook.empty())
    {
        std::cout << WarningMsg("您的通讯录是空的！ 加一些人 ")
                  << WarningMsg("首先.")
                  << std::endl;

        return true;
    }

    return false;
}

void deleteFromAddressBook()
{
    auto addressBook = getAddressBook();

    if (isAddressBookEmpty(addressBook))
    {
        return;
    }

    while (true)
    {
        std::cout << InformationMsg("注意：您可以随时输入“取消” ")
                  << InformationMsg("取消删除")
                  << std::endl
                  << std::endl;

        std::string friendlyName;

        std::cout << InformationMsg("您要输入什么通讯录 ")
                  << InformationMsg("删除?: ");

        std::getline(std::cin, friendlyName);
        trim(friendlyName);

        if (friendlyName == "取消")
        {
            std::cout << WarningMsg("取消删除.") << std::endl;
            return;
        }

        auto it = std::find(addressBook.begin(), addressBook.end(),
                            AddressBookEntry(friendlyName));

        if (it != addressBook.end())
        {
            addressBook.erase(it);

            if (saveAddressBook(addressBook))
            {
                std::cout << std::endl
                          << SuccessMsg("此项已从中删除 ")
                          << SuccessMsg("您的通讯录!")
                          << std::endl;
            }

            return;
        }

        std::cout << std::endl
                  << WarningMsg("找不到名称为的用户 ")
                  << InformationMsg(friendlyName)
                  << WarningMsg(" 在您的通讯录中!")
                  << std::endl
                  << std::endl;

        bool list = confirm("您想列出您的所有人吗 "
                            "地址簿?");

        std::cout << std::endl;

        if (list)
        {
            listAddressBook();
        }
    }
}

void listAddressBook()
{
    auto addressBook = getAddressBook();

    if (isAddressBookEmpty(addressBook))
    {
        return;
    }

    int index = 1;

    for (const auto &i : addressBook)
    {
        std::cout << InformationMsg("通讯录条目 #")
                  << InformationMsg(std::to_string(index))
                  << InformationMsg(":")
                  << std::endl
                  << std::endl
                  << InformationMsg("友好名称: ")
                  << std::endl
                  << SuccessMsg(i.friendlyName)
                  << std::endl
                  << std::endl
                  << InformationMsg("地址: ")
                  << std::endl
                  << SuccessMsg(i.address)
                  << std::endl
                  << std::endl;

        if (i.paymentID != "")
        {
            std::cout << InformationMsg("付款编号: ")
                      << std::endl
                      << SuccessMsg(i.paymentID)
                      << std::endl
                      << std::endl
                      << std::endl;
        }
        else
        {
            std::cout << std::endl;
        }

        index++;
    }
}

AddressBook getAddressBook()
{
    AddressBook addressBook;

    std::ifstream input(WalletConfig::addressBookFilename);

    /* If file exists, read current values */
    if (input)
    {
        std::stringstream buffer;
        buffer << input.rdbuf();
        input.close();

        CryptoNote::loadFromJson(addressBook, buffer.str());
    }

    return addressBook;
}

bool saveAddressBook(AddressBook addressBook)
{
    std::string jsonString = CryptoNote::storeToJson(addressBook);

    std::ofstream output(WalletConfig::addressBookFilename);

    if (output)
    {
        output << jsonString;
    }
    else
    {
        std::cout << WarningMsg("无法将地址簿保存到磁盘!")
                  << std::endl
                  << WarningMsg("检查您是否能够将文件写入到 ")
                  << WarningMsg("当前目录.") << std::endl;

        return false;
    }

    output.close();

    return true;
}
