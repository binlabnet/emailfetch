/*
 * EmailFetcherConfig.cpp
 *
 *  Created on: Nov 4, 2017
 *      Author: paulc
 */
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <stdio.h>
#include <cstddef>
#include <string>
#include <iostream>
#include <iomanip>
#include <errno.h>
#include <fstream>
#include <ios>
#include <sstream>
#include <regex>

#include <iterator>
#include <pwd.h>
#include <grp.h>
#include <errno.h>

#include "EmailFetcherConfig.h"

#include "config.h"

using namespace std;
using namespace Aws;

struct program_defaults defaults =
{
   5000,
   5000,
   {
       "./mail/.%f",
       "./mail/%d/%u@%d/"
   },
   {
       "./mail/slot/%f",
       8301,
       "eapfastemail.ucdp.thomsonreuters.com",
       "./cfg/ucdp_eapfastemail.pem",
       "password",
       "eapfastemail",
       "news_eapfe",
       "EmailML"
   }
};

void get_program_defaults(Utils::Json::JsonValue &jv, program_defaults &defaults)
{
    Utils::Json::JsonValue jdefaults = jv.GetObject("defaults");
    // who knows, maybe we'll need this to be thread safe
    int sz = 0, ret = 0;
    if (jdefaults.ValueExists("user"))
    {
        string user = jdefaults.GetObject("user").AsString().c_str();
        if ((sz = sysconf(_SC_GETPW_R_SIZE_MAX)) > 0)
        {
            char buf[sz];
            struct passwd pwent, *ppent = &pwent;
            if ((ret = getpwnam_r(user.c_str(), &pwent, buf, sz, &ppent)) == 0 && ppent)
                defaults.euser = pwent.pw_uid;
            else
            {
                int lerrno = errno;
                char ebuf[1024], *p;
                if (lerrno && (p = strerror_r(lerrno, ebuf, 1024)))
                    cout << "CONFIG: Error: Failed to get uid for user " << user << " error (" << p << ")" << endl;
                else if (lerrno)
                    cout << "CONFIG: Error: Failed to get uid for user " << user << " errno (" << lerrno << ")" << endl;
                else
                    cout << "CONFIG: Error: Failed to get uid for user " << user << " (user not found)" << endl;
            }
        }
    }
    if (jdefaults.ValueExists("group"))
    {
        string grp = jdefaults.GetObject("group").AsString().c_str();
        if ((sz = sysconf(_SC_GETGR_R_SIZE_MAX)) > 0)
        {
            char buf[sz];
            struct group grent, *pgent;
            if ((ret = getgrnam_r(grp.c_str(), &grent, buf, sz, &pgent)) == 0 && pgent)
                defaults.egroup = grent.gr_gid;
            else
            {
                int lerrno = errno;
                char ebuf[1024], *p;
                if (lerrno && (p = strerror_r(lerrno, ebuf, 1024)))
                    cout << "CONFIG: Error: Failed to get gid for group " << grp << " error (" << p << ")" << endl;
                else if (lerrno)
                    cout << "CONFIG: Error: Failed to get gid for group " << grp << " errno (" << lerrno << ")" << endl;
                else
                    cout << "CONFIG: Error: Failed to get gid for group " << grp << " (group not found)" << endl;
            }
        }
    }
    if (jdefaults.ValueExists("mailbox"))
    {
        Utils::Json::JsonValue mailboxdefaults = jdefaults.GetObject("mailbox");
        if (mailboxdefaults.ValueExists("publicdir"))
            defaults.mailbox_defaults.publicdir = mailboxdefaults.GetString("publicdir").c_str();
        if (mailboxdefaults.ValueExists("userdir"))
            defaults.mailbox_defaults.userdir = mailboxdefaults.GetString("userdir").c_str();
    }
    if (jdefaults.ValueExists("UCDP"))
    {
        Utils::Json::JsonValue ucdpdefaults = jdefaults.GetObject("UCDP");
        if (ucdpdefaults.ValueExists("port"))
            defaults.UCDP_defaults.port = ucdpdefaults.GetInteger("port");
        if (ucdpdefaults.ValueExists("snihostname"))
            defaults.UCDP_defaults.snihostname = ucdpdefaults.GetString("snihostname").c_str();
        if (ucdpdefaults.ValueExists("certificate"))
            defaults.UCDP_defaults.certificate = ucdpdefaults.GetString("certificate").c_str();
        if (ucdpdefaults.ValueExists("certificatepassword"))
            defaults.UCDP_defaults.certificatepassword = ucdpdefaults.GetString("certificatepassword").c_str();
        if (ucdpdefaults.ValueExists("trclientid"))
            defaults.UCDP_defaults.trclientid = ucdpdefaults.GetString("trclientid").c_str();
        if (ucdpdefaults.ValueExists("trfeedid"))
            defaults.UCDP_defaults.trfeedid = ucdpdefaults.GetString("trfeedid").c_str();
        if (ucdpdefaults.ValueExists("trmessagetype"))
            defaults.UCDP_defaults.trmessagetype = ucdpdefaults.GetString("trmessagetype").c_str();
        if (ucdpdefaults.ValueExists("workdir"))
            defaults.UCDP_defaults.workdir = ucdpdefaults.GetString("workdir").c_str();
    }
}

string replace_all(string str, const string &from, const string &to)
{
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos)
    {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // Handles case where 'to' is a substring of 'from'
    }
    return str;
}

string create_location(string fmt, string &user, string &name, string &domainname, string destination)
{
    string result = fmt;

    result = replace_all(result, "%u", user);        // replace all "%u with the user's name
    result = replace_all(result, "%f", name);        // replace all "%f with the feed name
    result = replace_all(result, "%d", domainname);  // replace all "%d with the domain name
    result = replace_all(result, "%a", destination); // replace all "%a with the destination ip or host name

    std::size_t n = result.find_last_of('/');
    if (n != std::string::npos && n != result.length() - 1)
        result += "/";

    return result;
}

string create_public_mailbox_location(program_defaults &defaults, string &name, string &domainname)
{
    return create_location(defaults.mailbox_defaults.publicdir, name, name, domainname, "");
}

string create_user_mailbox_location(program_defaults &defaults, string &user, string &name, string &domainname)
{
    return create_location(defaults.mailbox_defaults.userdir, user, name, domainname, "");
}

void get_mailbox_config(Utils::Json::JsonValue &jv, config_list &config)
{
    Utils::Array<Utils::Json::JsonValue> arr = jv.GetArray("mailbox");

    for (int i = 0; i < arr.GetLength(); i++)
    {
        config_item *pitem = new config_item;

        if (arr[i].ValueExists("bucket"))
            pitem->bucket = arr[i].GetString("bucket").c_str();
        else
        {
            cout << "CONFIG: Error: no AWS S3 bucket specified for this downloader -  ignoring" << endl;
            continue;
        }

        if (arr[i].ValueExists("email"))
        {
            string email = arr[i].GetString("email").c_str();
            istringstream e(email);
            getline(e, pitem->name, '@');
            getline(e, pitem->domainname, '@');
        }
        else
        {
            if (arr[i].ValueExists("name"))
                pitem->name = arr[i].GetString("name").c_str();
            else
            {
                cout << "CONFIG: Error: Cannot determine the email address for this downloader (name missing)" << endl;
                continue;
            }
            if (arr[i].ValueExists("domainname"))
                pitem->domainname = arr[i].GetString("domainname").c_str();
            else
            {
                cout << "CONFIG: Error: Cannot determine the email address for this downloader (domainname missing)" << endl;
                continue;
            }
        }

        if (arr[i].ValueExists("description"))
            pitem->description = arr[i].GetString("description").c_str();
        else
            pitem->description = pitem->name + "@" + pitem->domainname;

        if (arr[i].ValueExists("topic_arn"))
            pitem->topic_arn = arr[i].GetString("topic_arn").c_str();
        else
        {
            cout << "CONFIG: Error: no topic arn specified for downloader " << pitem->name << endl;
            continue;
        }

        Utils::Array<Utils::Json::JsonValue> locations = arr[i].GetArray("locations");
        int nlocs = locations.GetLength();
        for (int j = 0; j < nlocs; j++)
        {
            if (locations[j].ValueExists("mailbox"))
            {
                location_type *ploc = new location_type;
                bool is_mailbox = locations[j].GetBool("mailbox");
                if (is_mailbox)
                {
                    ploc->type = NONSLOT;
                    pitem->has_nonslot_workflow = true;
                    bool is_public = false;
                    string user = "";
                    string workdir = "";

                    if (locations[j].ValueExists("public"))
                        is_public = locations[j].GetBool("public");
                    if (locations[j].ValueExists("user"))
                        user = locations[j].GetString("user").c_str();
                    if (locations[j].ValueExists("workdir"))
                        workdir = locations[j].GetString("workdir").c_str();

                    if (is_public)
                        ploc->destination = create_public_mailbox_location(defaults, pitem->name, pitem->domainname);
                    else
                    {
                        if (user != "")
                        {
                            ploc->mailbox.user = user;
                            ploc->destination = create_user_mailbox_location(defaults, ploc->mailbox.user, pitem->name,pitem->domainname);
                        }
                        else
                        {
                            if (workdir != "")
                            {
                                string fmt = workdir;
                                ploc->mailbox.user = "";
                                ploc->destination = create_location(fmt, user, pitem->name, pitem->domainname, "");
                            }
                            else
                            {
                                cout << "CONFIG: Error: Unable to find user or workdir element for mailbox location - ignoring this location" << endl;
                                continue;
                            }
                        }
                    }
                }
                else // it goes to a REST post
                {
                    string addr = "";

                    ploc->type = SLOT;
                    // SNI is the default for UCDP, and UCDP is the default system to post to
                    if (locations[j].ValueExists("UCDP"))
                        ploc->rest.UCDP = locations[j].GetBool("UCDP");
                    else
                        ploc->rest.UCDP = true;

                    if (!ploc->rest.UCDP && locations[j].ValueExists("url"))
                    {
                        string fmt = locations[j].GetString("url").c_str();
                        string user = "";
                        ploc->destination = create_location(fmt, user, pitem->name, pitem->domainname, "");

                        // extract the ip or host from the url, for substitutions
                        std::smatch sm;
                        std::regex e_addr   ("^(http[s]?:)//([^/]+)/(.*)");
                        std::regex_match(ploc->destination, sm, e_addr);
                        if (sm.size() > 0)
                            addr = sm[2];

                    }
                    else if (ploc->rest.UCDP && locations[j].ValueExists("ip"))  // these are the minimum required params for SNI
                    {
                        ploc->destination = locations[j].GetString("ip").c_str();
                        addr = ploc->destination;

                        if (locations[j].ValueExists("certificate"))
                            ploc->rest.certificate = locations[j].GetString("certificate").c_str();
                        else
                            ploc->rest.certificate = defaults.UCDP_defaults.certificate;

                        if (locations[j].ValueExists("certificatepassword"))
                            ploc->rest.certificatepassword = locations[j].GetString("certificatepassword").c_str();
                        else
                            ploc->rest.certificatepassword = defaults.UCDP_defaults.certificatepassword;

                        if (locations[j].ValueExists("port"))
                            ploc->rest.port = locations[j].GetInteger("port");
                        else
                            ploc->rest.port = defaults.UCDP_defaults.port;

                        if (locations[j].ValueExists("trclientid"))
                            ploc->rest.trclientid = locations[j].GetString("trclientid").c_str();
                        else
                            ploc->rest.trclientid = defaults.UCDP_defaults.trclientid;

                        if (locations[j].ValueExists("trfeedid"))
                            ploc->rest.trfeedid = locations[j].GetString("trfeedid").c_str();
                        else
                            ploc->rest.trfeedid = defaults.UCDP_defaults.trfeedid;

                        if (locations[j].ValueExists("trmessagetype"))
                            ploc->rest.trmessagetype = locations[j].GetString("trmessagetype").c_str();
                        else
                            ploc->rest.trmessagetype = defaults.UCDP_defaults.trmessagetype;
                    }
                    else
                    {
                        cout << "CONFIG: Error: no URL or IP/HOST/PORT specified for REST destination - ignoring location in " << pitem->name << endl;
                        continue;
                    }

                    if (locations[j].ValueExists("workdir"))
                    {
                        string fmt = locations[j].GetString("workdir").c_str();
                        ploc->rest.workdir = create_location(fmt, ploc->mailbox.user, pitem->name, pitem->domainname, addr);
                    }
                    else
                        ploc->rest.workdir = create_location(defaults.UCDP_defaults.workdir, ploc->mailbox.user, pitem->name, pitem->domainname, addr);
                }

                pitem->locations.push_back(*ploc);
            }
            else
                cout << "CONFIG: Error: required element \"mailbox\" boolean element not specified - ignoring location in " << pitem->name << endl;
        }

        pitem->enabled = arr[i].GetBool("enabled");
        config.push_back(*pitem);
    }
}

int get_config(const string location, program_defaults &defaults, config_list &config)
{
    string jstr;

    ifstream infile(location, ifstream::in);
    streamsize n;
    char buf[512];
    do
    {
        n = infile.readsome(buf, 511);
        buf[n] = '\0';
        jstr += buf;
    } while (infile.good() && n);
    infile.close();

    istringstream inp(jstr);
    Utils::Json::JsonValue jv(inp);

    // program defaults
    get_program_defaults(jv, defaults);

    // mailbox config
    get_mailbox_config(jv, config);

    return 0;
}

void print_config(config_list &mailboxconfig)
{
    for (auto &item : mailboxconfig)
    {
        cout << "CONFIG: " << item.description << endl;
        cout << "CONFIG:    " << item.name << '@' << item.domainname;
        cout << (item.enabled ? " is enabled" : " is disabled");
        cout << (item.has_nonslot_workflow ? " and has non-slot workflow\n" : "\n");
        cout << "CONFIG:    " << "S3 Bucket:     " << item.bucket << endl;
        cout << "CONFIG:    " << "SNS Topic ARN: " << item.topic_arn << endl;
        cout << "CONFIG:    " << "Locations:" << endl;
        int i = 0;
        for (auto &loc : item.locations)
        {
            ++i;
            if (loc.type == SLOT)
            {
                if (loc.rest.UCDP)
                {
                    cout << "CONFIG:       " << i << ". Slot:     " << "SLOT: UCDP destination" << endl;
                    cout << "CONFIG:       " << i << ". Slot:     " << "   IP:          " << loc.destination << endl;
                    cout << "CONFIG:       " << i << ". Slot:     " << "   Port:        " << loc.rest.port << endl;
                    cout << "CONFIG:       " << i << ". Slot:     " << "   ClientId:    " << loc.rest.trclientid << endl;
                    cout << "CONFIG:       " << i << ". Slot:     " << "   FeedId:      " << loc.rest.trfeedid << endl;
                    cout << "CONFIG:       " << i << ". Slot:     " << "   MessageType: " << loc.rest.trmessagetype << endl;
                }
                else
                {
                    cout << "CONFIG:       " << i << ". Slot:     " << "SLOT: REST URL destination" << endl;
                    cout << "CONFIG:       " << i << ". Slot:     " << "   URL:         " << loc.destination << endl;
                }
                cout << "CONFIG:       " << i << ". Slot:     " << "   workdir:     " << loc.rest.workdir << endl;
            }
            else
            {
                cout << "CONFIG:       " << i << ". Slot:     " << "NON-SLOT Mailbox Destination" << endl;
                if (loc.mailbox.user != "")
                    cout << "CONFIG:       " << i << ". Non-slot: " << "   user:        " << loc.mailbox.user << endl;
                cout << "CONFIG:       " << i << ". Non-slot: " << "   destination: " << loc.destination << endl;
            }
        }
    }
}

