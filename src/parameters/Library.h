#pragma once

#include "../dsp/Sequencer.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace hkr
{

inline bool readXmlAttr (const std::string& tag, const char* key, std::string& out)
{
    const std::string k = std::string (key) + "=\"";
    const auto pos = tag.find (k);
    if (pos == std::string::npos)
        return false;
    const auto start = pos + k.size();
    const auto end = tag.find ('"', start);
    if (end == std::string::npos)
        return false;
    out.assign (tag, start, end - start);
    return true;
}

inline std::string writeBankXml (const SlotData slots[Sequencer::numBanks][Sequencer::patternsPerBank])
{
    std::string o = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<HKR5BANK>\n";
    for (int b = 0; b < Sequencer::numBanks; ++b)
        for (int p = 0; p < Sequencer::patternsPerBank; ++p)
        {
            const auto& s = slots[b][p];
            char head[64];
            std::snprintf (head, sizeof (head), "  <PAT b=\"%d\" p=\"%d\" len=\"%d\">\n",
                           b, p, (int) s.len);
            o += head;
            for (int lane = 0; lane < numLanes; ++lane)
            {
                char lh[80];
                std::snprintf (lh, sizeof (lh), "    <LANE i=\"%d\" loop=\"%d\">",
                               lane, (int) s.loop[lane]);
                o += lh;
                for (int i = 0; i < SlotData::maxSteps; ++i)
                {
                    const int v = getStep (s, lane, i);
                    if (v == 0)
                        continue;
                    char st[48];
                    std::snprintf (st, sizeof (st), "<S i=\"%d\" v=\"%d\"/>", i, v);
                    o += st;
                }
                o += "</LANE>\n";
            }
            o += "  </PAT>\n";
        }
    o += "</HKR5BANK>\n";
    return o;
}

inline bool readBankXml (const std::string& xml,
                         SlotData slots[Sequencer::numBanks][Sequencer::patternsPerBank])
{
    if (xml.find ("<HKR5BANK") == std::string::npos)
        return false;

    for (int b = 0; b < Sequencer::numBanks; ++b)
        for (int p = 0; p < Sequencer::patternsPerBank; ++p)
            fillFactory (slots[b][p], b, p);

    std::size_t pos = 0;
    while ((pos = xml.find ("<PAT", pos)) != std::string::npos)
    {
        const auto tagEnd = xml.find ('>', pos);
        if (tagEnd == std::string::npos)
            return false;
        const auto open = xml.substr (pos, tagEnd - pos);
        pos = tagEnd + 1;
        if (open.find ("<PAT") == std::string::npos)
            continue;

        std::string bs, ps, lens;
        if (! readXmlAttr (open, "b", bs) || ! readXmlAttr (open, "p", ps))
            continue;
        const int b = std::atoi (bs.c_str());
        const int p = std::atoi (ps.c_str());
        if (b < 0 || b >= Sequencer::numBanks || p < 0 || p >= Sequencer::patternsPerBank)
            continue;

        auto& slot = slots[b][p];
        slot = SlotData();
        if (readXmlAttr (open, "len", lens))
            slot.len = (uint8_t) clampLen (std::atoi (lens.c_str()));

        const auto close = xml.find ("</PAT>", pos);
        const auto blockEnd = close == std::string::npos ? xml.size() : close;
        std::size_t lp = pos;
        while ((lp = xml.find ("<LANE", lp)) != std::string::npos && lp < blockEnd)
        {
            const auto end = xml.find ('>', lp);
            if (end == std::string::npos || end > blockEnd)
                break;
            const auto tag = xml.substr (lp, end - lp);
            lp = end + 1;
            std::string is, loops;
            if (! readXmlAttr (tag, "i", is))
                continue;
            const int lane = std::atoi (is.c_str());
            if (lane < 0 || lane >= numLanes)
                continue;
            if (readXmlAttr (tag, "loop", loops))
                slot.loop[lane] = (uint8_t) std::clamp (std::atoi (loops.c_str()), 1, SlotData::maxSteps);

            const auto laneClose = xml.find ("</LANE>", lp);
            const auto laneEnd = laneClose == std::string::npos || laneClose > blockEnd ? blockEnd : laneClose;
            std::size_t sp = lp;
            while ((sp = xml.find ("<S", sp)) != std::string::npos && sp < laneEnd)
            {
                const auto se = xml.find ('>', sp);
                if (se == std::string::npos || se > laneEnd)
                    break;
                const auto st = xml.substr (sp, se - sp);
                sp = se + 1;
                std::string ii, vv;
                if (! readXmlAttr (st, "i", ii) || ! readXmlAttr (st, "v", vv))
                    continue;
                setStep (slot, lane, std::atoi (ii.c_str()), std::atoi (vv.c_str()));
            }
            lp = laneEnd;
        }
        pos = blockEnd;
    }
    return true;
}

} // namespace hkr
