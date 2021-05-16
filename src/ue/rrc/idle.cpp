//
// This file is a part of UERANSIM open source project.
// Copyright (c) 2021 ALİ GÜNGÖR.
//
// The software and all associated files are licensed under GPL-3.0
// and subject to the terms and conditions defined in LICENSE file.
//

#include "task.hpp"

#include <algorithm>

#include <lib/rrc/encode.hpp>
#include <ue/nas/task.hpp>

namespace nr::ue
{

void UeRrcTask::performCellSelection()
{
    if (m_state == ERrcState::RRC_CONNECTED)
        return;

    if (!lookForSuitableCell())
        lookForAcceptableCell();
}

bool UeRrcTask::lookForSuitableCell()
{
    Plmn selectedPlmn = m_base->shCtx.selectedPlmn.get();
    if (!selectedPlmn.hasValue())
        return false;

    int lastCell = m_base->shCtx.currentCell.get<int>([](auto &value) { return value.cellId; });

    int outOfPlmnCells = 0;
    int sib1MissingCells = 0;
    int barredCells = 0;
    int reservedCells = 0;

    std::vector<int> candidates;

    for (auto &item : m_cellDesc)
    {
        auto &cell = item.second;

        if (!cell.sib1.hasSib1)
        {
            sib1MissingCells++;
            continue;
        }

        if (cell.sib1.plmn != selectedPlmn)
        {
            outOfPlmnCells++;
            continue;
        }

        if (cell.mib.hasMib)
        {
            if (cell.mib.isBarred)
            {
                barredCells++;
                continue;
            }
        }

        if (cell.sib1.isReserved)
        {
            reservedCells++;
            continue;
        }

        // TODO: Check TAI if forbidden by service area or forbidden list
        // TODO: Do we need to check by access identity

        // It seems suitable
        candidates.push_back(item.first);
    }

    if (candidates.empty())
    {
        m_logger->err("Cell selection failed in [%d] cells. [%d] out of PLMN, [%d] no SI, [%d] reserved, [%d] barred",
                      static_cast<int>(m_cellDesc.size()), outOfPlmnCells, sib1MissingCells, reservedCells,
                      barredCells);
        return false;
    }

    // Order candidates by signal strength
    std::sort(candidates.begin(), candidates.end(), [this](int a, int b) {
        auto &cellA = m_cellDesc[a];
        auto &cellB = m_cellDesc[b];
        return cellB.dbm < cellA.dbm;
    });

    auto &selectedId = candidates[0];
    auto &selectedCell = m_cellDesc[selectedId];

    CurrentCellInfo cellInfo;
    cellInfo.cellId = selectedId;
    cellInfo.plmn = selectedCell.sib1.plmn;
    cellInfo.tac = selectedCell.sib1.tac;
    cellInfo.category = ECellCategory::SUITABLE_CELL;

    m_base->shCtx.currentCell.set(cellInfo);

    if (lastCell != selectedId)
    {
        m_logger->info("Selected cell id[%d] category[SUITABLE]", selectedId);
        // TODO: Notify RLS
        m_base->nasTask->push(new NwUeRrcToNas(NwUeRrcToNas::NAS_NOTIFY));
    }
    return true;
}

bool UeRrcTask::lookForAcceptableCell()
{
    // TODO
    return false;
}

} // namespace nr::ue