#include "playerbot/playerbot.h"
#include "CheckMailAction.h"
#include "Mails/Mail.h"
#include "playerbot/strategy/values/ItemUsageValue.h"

#include "playerbot/PlayerbotAIConfig.h"
using namespace ai;

bool CheckMailAction::Execute(Event& event)
{
    WorldPacket p;
    bot->GetSession()->HandleQueryNextMailTime(p);   

    std::list<uint32> ids;

    PlayerMails mails;

    //Fetch mails first and then loop over them to prevent needing to check mails sent to self.
    for (PlayerMails::iterator i = bot->GetMailBegin(); i != bot->GetMailEnd(); ++i)
    {
        mails.push_back(*i);
    }

    for (auto & mail : mails)
    {
        if (!mail || mail->state == MAIL_STATE_DELETED)
            continue;

        Player* owner = sObjectMgr.GetPlayer(ObjectGuid(HIGHGUID_PLAYER, mail->sender));
        if (!owner)
            continue;

        uint32 account = sObjectMgr.GetPlayerAccountIdByGUID(owner->GetObjectGuid());
        if (sPlayerbotAIConfig.IsInRandomAccountList(account))
            continue;

        ProcessMail(mail, owner);
        ids.push_back(mail->messageID);
        mail->state = MAIL_STATE_DELETED;
    }

    for (std::list<uint32>::iterator i = ids.begin(); i != ids.end(); ++i)
    {
        uint32 id = *i;
        bot->SendMailResult(id, MAIL_DELETED, MAIL_OK);
        CharacterDatabase.PExecute("DELETE FROM mail WHERE id = '%u'", id);
        CharacterDatabase.PExecute("DELETE FROM mail_items WHERE mail_id = '%u'", id);
        bot->RemoveMail(id);
    }

    return true;
}

bool CheckMailAction::isUseful()
{
    if (ai->GetMaster() || !bot->GetMailSize() || bot->InBattleGround())
        return false;

    return true;
}


bool CheckMailAction::ShouldProcessMail(Mail* mail)
{
    if (mail->items.empty())
    {
        return false;
    }

    if (mail->subject.find("Item(s) you asked for") != std::string::npos)
        return false;

    return true;
}

void CheckMailAction::SendReturnMail(Mail* mail, Player* owner)
{
    for (MailItemInfoVec::iterator i = mail->items.begin(); i != mail->items.end(); ++i)
    {
        Item *item = bot->GetMItem(i->item_guid);
        if (!item)
            continue;

        std::ostringstream body;
        body << "Hello, " << owner->GetName() << ",\n";
        body << "\n";
        body << "Here are the item(s) you've sent me by mistake";
        body << "\n";
        body << "Thanks,\n";
        body << bot->GetName() << "\n";

        MailDraft draft("Item(s) you've sent me", body.str());
        draft.AddItem(item);
        bot->RemoveMItem(i->item_guid);
        draft.SendMailTo(MailReceiver(owner), MailSender(bot));
        return;
    }
}

void CheckMailAction::ProcessMail(Mail* mail, Player* owner)
{
    if (!ShouldProcessMail(mail))
        return;

    for (auto& item : mail->items)
    {
        ItemQualifier itemQualifier(item);
        ItemUsageValue itemUsageValue(bot->GetAI(), "item usage");
        ItemUsage itemUsage = itemUsageValue.Calculate(itemQualifier);

        if (itemUsage == ItemUsage::ITEM_USAGE_NONE)
        {
            // Item is not useful, SendReturnMail
            SendReturnMail(mail, owner);
            continue;
        }

        // Item is useful, get item from mail.
        Item *item = bot->GetMItem(i->item_guid);
        item->SetOwnerGUID(bot->GetObjectGuid());
    }
}
