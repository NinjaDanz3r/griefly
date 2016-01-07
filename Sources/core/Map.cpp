#include <math.h>
#include <sstream>
#include <iostream>
#include <assert.h>

#include <QDebug>

#include "Map.h"
#include "objects/MainObject.h"
#include "objects/Tile.h"
#include "Manager.h"
#include "MobPosition.h"
#include "ObjectFactory.h"
#include "VisiblePoints.h"
//#include "SpaceTurf.h"
//#include "MetalWall.h"
#include "helpers.h"
//#include "Floor.h"
//#include "Door.h"
//#include "Grille.h"
//#include "Glass.h"
//#include "Item.h"
//#include "Shard.h"
//#include "Weldingtool.h"
//#include "Lattice.h"
//#include "FloorTile.h"
//#include "Materials.h"
//#include "ElectricTools.h"

#include "helpers.h"
#include "objects/Creator.h"
#include "AutogenMetadata.h"
#include "representation/Representation.h"

void MapMaster::FillAtmosphere()
{
    for (int z = 0; z < GetMapD(); ++z)
        for (int x = 0; x < GetMapW(); ++x)
            for (int y = 0; y < GetMapH(); ++y)
                if (   squares[x][y][z]->GetTurf()
                    && squares[x][y][z]->GetTurf()->GetAtmosState() != SPACE
                    && squares[x][y][z]->GetTurf()->GetAtmosState() != NON_SIMULATED
                    && CanPass(squares[x][y][z]->GetPassable(D_ALL), Passable::AIR))
                {
                    auto a = squares[x][y][z]->GetAtmosHolder();
                    a->AddGase(NYTROGEN, 750);
                    a->AddGase(OXYGEN, 230);
                    a->AddGase(CO2, 1);
                    a->AddEnergy(1000);
                }
}

void MapMaster::SaveToMapGen(const std::string& name)
{
    std::fstream sfile;
    sfile.open(name, std::ios_base::out | std::ios_base::trunc);
    if(sfile.fail()) 
    {
        SYSTEM_STREAM << "Error open " << name << std::endl; 
        return;
    }

    sfile << GetMapW() << std::endl;
    sfile << GetMapH() << std::endl;
    sfile << GetMapD() << std::endl;
//    sfile << GetCreator() << std::endl;

    std::map<std::string, std::string> dummy;

    for (int z = 0; z < GetMapD(); ++z)
        for (int x = 0; x < GetMapW(); ++x)
            for (int y = 0; y < GetMapH(); ++y)
            {
                if (auto t = squares[x][y][z]->GetTurf())
                {
                    sfile << t->T_ITEM() << " ";
                    sfile << x << " ";
                    sfile << y << " ";
                    sfile << z << " ";
                    sfile << std::endl;
                }
                auto& il = squares[x][y][z]->GetInsideList();
                for (auto it = il.begin(); it != il.end(); ++it)
                {
                    sfile << (*it)->T_ITEM() << " ";
                    sfile << x << " ";
                    sfile << y << " ";
                    sfile << z << " ";
                    sfile << std::endl;
                }
                std::stringstream ss;
                WrapWriteMessage(ss, dummy);
                sfile << ss.str();
            }
}

void MapMaster::LoadFromMapGen(const std::string& name)
{
    GetFactory().ClearMap();

    std::fstream sfile;
    sfile.open(name, std::ios_base::in);
    if(sfile.fail()) 
    {
        SYSTEM_STREAM << "Error open " << name << std::endl; 
        return;
    }

    std::stringstream ss;

    sfile.seekg (0, std::ios::end);
    std::streamoff length = sfile.tellg();
    sfile.seekg (0, std::ios::beg);
    char* buff = new char[static_cast<size_t>(length)];

    sfile.read(buff, length);
    sfile.close();
    ss.write(buff, length);
    delete[] buff;

    GetFactory().BeginWorldCreation();

    int x, y, z;
    ss >> x;
    ss >> y;
    ss >> z;

 //   size_t creator;
//    sfile >> creator;

    MakeTiles(x, y, z);

   // qDebug() << "Begin loading cycle";
    while (ss)
    {
        std::string t_item;
        size_t x, y, z;
        ss >> t_item;
        if (!ss)
        {
            continue;
        }
        ss >> x;
        if (!ss)
        {
            continue;
        }
        ss >> y;
        if (!ss)
        {
            continue;
        }
        ss >> z;
        if (!ss)
        {
            continue;
        }

        auto i = GetFactory().Create<IOnMapObject>(t_item);

        std::map<std::string, std::string> variables;
        WrapReadMessage(ss, variables);

        for (auto it = variables.begin(); it != variables.end(); ++it)
        {
            if ((it->second.size() == 0) || (it->first.size() == 0))
            {
                continue;
            }
            std::stringstream local_variable;
            local_variable << it->second;

            //qDebug() << it->second.c_str();

            get_setters_for_types()[t_item][it->first](i.operator*(), local_variable);
        }

        if (id_ptr_on<ITurf> t = i)
        {
            if (squares[x][y][z]->GetTurf())
            {
                SYSTEM_STREAM << "DOUBLE TURF!" << std::endl;
            }
            squares[x][y][z]->SetTurf(t);
        }
        else
        {
            squares[x][y][z]->AddItem(i);
        }
    }
   GetFactory().FinishWorldCreation();
}

void MapMaster::Represent()
{
    if(!GetVisible())
    {
        return;
    }

    for(int i = 0; i < MAX_LEVEL; ++i)
    {
        auto it2 = GetVisible()->begin();
        while(it2 != GetVisible()->end())
        {
            auto sq = squares[it2->posx][it2->posy][it2->posz];
            auto& in_list = sq->GetInsideList();

            for (auto list_it = in_list.begin(); list_it != in_list.end(); ++list_it)
            {
                if ((*list_it)->v_level == i)
                {
                    (*list_it)->Represent();
                }
            }

            auto trf = squares[it2->posx][it2->posy][it2->posz]->GetTurf();
            if (trf.valid() && trf->v_level == i)
            {
                trf->Represent();
            }
            ++it2;
        }
    }
    auto it2 = GetVisible()->begin();
    while(it2 != GetVisible()->end())
    {
        auto sq = squares[it2->posx][it2->posy][it2->posz];
        auto& in_list = sq->GetInsideList();

        for (auto list_it = in_list.begin(); list_it != in_list.end(); ++list_it)
        {
            if ((*list_it)->v_level >= MAX_LEVEL)
            {
                (*list_it)->Represent();
            }
        }

        auto trf = squares[it2->posx][it2->posy][it2->posz]->GetTurf();
        if (trf.valid() && trf->v_level >= MAX_LEVEL)
        {
            trf->Represent();
        }
        ++it2;
    }
}

void MapMaster::GenerateFrame()
{
    if(!GetVisible())
    {
        return;
    }

    Represent();

    /*for (auto it = GetVisible()->begin(); it != GetVisible()->end(); ++it)
    {
        auto sq = squares[it->posx][it->posy][it->posz];
        auto& in_list = sq->GetInsideList();

        for (auto list_it = in_list.begin(); list_it != in_list.end(); ++list_it)
        {
            Representation::Entity ent;
            ent.id = list_it->ret_id();
            ent.pos_x = it->posx;
            ent.pos_y = it->posy;
            ent.vlevel = (*list_it)->v_level;
            if (id_ptr_on<IMovable> mov = *list_it)
            {
                ent.dir = mov->GetDir();
            }
            ent.view = *((*list_it)->GetView());
            GetRepresentation().AddToNewFrame(ent);
        }

        auto trf = squares[it->posx][it->posy][it->posz]->GetTurf();
        Representation::Entity ent;
        ent.id = trf.ret_id();
        ent.pos_x = it->posx;
        ent.pos_y = it->posy;
        ent.vlevel = trf->v_level;
        //ent.dir = (*list_it)->GetDir();
        ent.view = *(trf->GetView());
        GetRepresentation().AddToNewFrame(ent);
    }*/

    GetMob()->GenerateInterfaceForFrame();


    // TODO: reset all shifts
    GetRepresentation().SetCameraForFrame(GetMob()->GetX(), GetMob()->GetY());
    GetRepresentation().Swap();
}

void MapMaster::MakeTiles(int new_map_x, int new_map_y, int new_map_z)
{
    ResizeMap(new_map_x, new_map_y, new_map_z);
    for(int x = 0; x < GetMapW(); x++)
    {
        for(int y = 0; y < GetMapH(); y++) 
        {
            for (int z = 0; z < GetMapD(); z++)
            {
                auto loc = GetFactory().Create<CubeTile>(CubeTile::T_ITEM_S());
                loc->SetPos(x, y, z);
                squares[x][y][z] = loc;
            }
        }
    }
}

void MapMaster::ResizeMap(int new_map_x, int new_map_y, int new_map_z)
{
    squares.resize(new_map_x);
    for (int x = 0; x < new_map_x; ++x)
    {
        squares[x].resize(new_map_y);
        for (int y = 0; y < new_map_y; ++y)
        {
            squares[x][y].resize(new_map_z);
        }
    }
    atmosphere.Resize(new_map_x, new_map_y);
}

MapMaster::MapMaster()
{

}

PassableLevel MapMaster::GetPassable(int posx, int posy, int posz, Dir direct)
{
    return squares[posx][posy][posz]->GetPassable(direct);
}

void MapMaster::switchDir(int& posx, int& posy, Dir direct, int num, bool back)//TODO: Remove back
{
    if(!back)
    {
        switch(direct)
        {
        case D_UP:
            posy -= num;
            return;
        case D_DOWN:
            posy += num;
            return;
        case D_LEFT:
            posx -= num;
            return;
        case D_RIGHT:
            posx += num;
            return;
        }
    }
    else
    {
        switch(direct)
        {
        case D_UP:
            posy += num;
            return;
        case D_DOWN:
            posy -= num;
            return;
        case D_LEFT:
            posx += num;
            return;
        case D_RIGHT:
            posx -= num;
            return;
        }
    }
}

bool MapMaster::IsTransparent(int posx, int posy, int posz)
{
    if (!helpers::check_borders(&posx, &posy, &posz))
    {
        return false;
    }
    return squares[posx][posy][posz]->IsTransparent();
}

MapMaster* map_master_ = 0;
MapMaster& GetMap()
{
    return *map_master_;
}

void SetMapMaster(MapMaster* map_master)
{
    map_master_ = map_master;
}

bool IsMapValid()
{
    return map_master_ != nullptr;
}\

std::list<point>* LOSfinder::calculateVisisble(std::list<point>* retlist, int posx, int posy, int posz)
{
    //auto retlist = new std::list<point>;
    clearLOS();
    //for(int level = 0; level < 
    point p = {posx, posy, posz};
    retlist->push_back(p);

    p.posx = posx + 1;
    p.posy = posy;
    if (helpers::check_borders(&p.posx, &p.posy, &p.posz))
        worklist.push_back(p);

    p.posx = posx + 1;
    p.posy = posy;
    if (helpers::check_borders(&p.posx, &p.posy, &p.posz))
        retlist->push_back(p);

    p.posx = posx + 1;
    p.posy = posy + 1;
    if (helpers::check_borders(&p.posx, &p.posy, &p.posz))
        worklist.push_back(p);

    p.posx = posx + 1;
    p.posy = posy + 1;
    if (helpers::check_borders(&p.posx, &p.posy, &p.posz))
        retlist->push_back(p);

    p.posx = posx;
    p.posy = posy + 1;
    if (helpers::check_borders(&p.posx, &p.posy, &p.posz))
        worklist.push_back(p);

    p.posx = posx;
    p.posy = posy + 1;
    if (helpers::check_borders(&p.posx, &p.posy, &p.posz))
        retlist->push_back(p);

    p.posx = posx - 1;
    p.posy = posy + 1;
    if (helpers::check_borders(&p.posx, &p.posy, &p.posz))
        worklist.push_back(p);

    p.posx = posx - 1;
    p.posy = posy + 1;
    if (helpers::check_borders(&p.posx, &p.posy, &p.posz))
        retlist->push_back(p);

    p.posx = posx - 1;
    p.posy = posy;
    if (helpers::check_borders(&p.posx, &p.posy, &p.posz))
        worklist.push_back(p);

    p.posx = posx - 1;
    p.posy = posy;
    if (helpers::check_borders(&p.posx, &p.posy, &p.posz))
        retlist->push_back(p);
    
    p.posx = posx - 1;
    p.posy = posy - 1;
    if (helpers::check_borders(&p.posx, &p.posy, &p.posz))
        worklist.push_back(p);

    p.posx = posx - 1;
    p.posy = posy - 1;
    if (helpers::check_borders(&p.posx, &p.posy, &p.posz))
        retlist->push_back(p);

    p.posx = posx;
    p.posy = posy - 1;
    if (helpers::check_borders(&p.posx, &p.posy, &p.posz))
        worklist.push_back(p);

    p.posx = posx;
    p.posy = posy - 1;
    if (helpers::check_borders(&p.posx, &p.posy, &p.posz))
        retlist->push_back(p);

    p.posx = posx + 1;
    p.posy = posy - 1;
    if (helpers::check_borders(&p.posx, &p.posy, &p.posz))
        worklist.push_back(p);

    p.posx = posx + 1;
    p.posy = posy - 1;
    if (helpers::check_borders(&p.posx, &p.posy, &p.posz))
        retlist->push_back(p);

    auto itr = worklist.begin();
    while(itr != worklist.end())
    {
        if(GetMap().IsTransparent(itr->posx, itr->posy, itr->posz)
            && itr->posx != posx - sizeHsq && itr->posx != posx + sizeHsq
            && itr->posy != posy - sizeWsq && itr->posy != posy + sizeWsq
            && 
            helpers::check_borders(&itr->posx, &itr->posy, &itr->posz))
            if(abs(itr->posx - posx) > abs(itr->posy - posy)) 
            if(itr->posx > posx)
            {
                p.posx = itr->posx + 1;
                p.posy = itr->posy;
                if (helpers::check_borders(&p.posx, &p.posy, &p.posz))
                    worklist.push_back(p);
                p.posx = itr->posx + 1;
                p.posy = itr->posy;
                if (helpers::check_borders(&p.posx, &p.posy, &p.posz))
                    retlist->push_back(p);
            }
            else 
            {
                p.posx = itr->posx - 1;
                p.posy = itr->posy;
                if (helpers::check_borders(&p.posx, &p.posy, &p.posz))
                    worklist.push_back(p);
                p.posx = itr->posx - 1;
                p.posy = itr->posy;
                if (helpers::check_borders(&p.posx, &p.posy, &p.posz))
                    retlist->push_back(p);
            }
            else if(abs(itr->posx - posx) < abs(itr->posy - posy)) 
            if(itr->posy > posy) 
            {
                p.posx = itr->posx;
                p.posy = itr->posy + 1;
                if (helpers::check_borders(&p.posx, &p.posy, &p.posz))
                    worklist.push_back(p);
                p.posx = itr->posx;
                p.posy = itr->posy + 1;
                if (helpers::check_borders(&p.posx, &p.posy, &p.posz))
                    retlist->push_back(p);
            }
            else 
            {
                p.posx = itr->posx;
                p.posy = itr->posy - 1;
                if (helpers::check_borders(&p.posx, &p.posy, &p.posz))
                    worklist.push_back(p);
                p.posx = itr->posx;
                p.posy = itr->posy - 1;
                if (helpers::check_borders(&p.posx, &p.posy, &p.posz))
                    retlist->push_back(p);
            }
            else
            if(itr->posx > posx)
            {
                if(itr->posy > posy)
                {
                p.posx = itr->posx + 1;
                p.posy = itr->posy + 1;
                if (helpers::check_borders(&p.posx, &p.posy, &p.posz))
                    worklist.push_back(p);
                p.posx = itr->posx + 1;
                p.posy = itr->posy + 1;
                if (helpers::check_borders(&p.posx, &p.posy, &p.posz))
                    retlist->push_back(p);
                p.posx = itr->posx + 1;
                p.posy = itr->posy;
                if (helpers::check_borders(&p.posx, &p.posy, &p.posz))
                    worklist.push_back(p);
                p.posx = itr->posx + 1;
                p.posy = itr->posy;
                if (helpers::check_borders(&p.posx, &p.posy, &p.posz))
                    retlist->push_back(p);
                p.posx = itr->posx;
                p.posy = itr->posy + 1;
                if (helpers::check_borders(&p.posx, &p.posy, &p.posz))
                    worklist.push_back(p);
                p.posx = itr->posx;
                p.posy = itr->posy + 1;
                if (helpers::check_borders(&p.posx, &p.posy, &p.posz))
                    retlist->push_back(p);
                }
                else
                {
                p.posx = itr->posx + 1;
                p.posy = itr->posy - 1;
                if (helpers::check_borders(&p.posx, &p.posy, &p.posz))
                    worklist.push_back(p);
                p.posx = itr->posx + 1;
                p.posy = itr->posy - 1;
                if (helpers::check_borders(&p.posx, &p.posy, &p.posz))
                    retlist->push_back(p);
                p.posx = itr->posx + 1;
                p.posy = itr->posy;
                if (helpers::check_borders(&p.posx, &p.posy, &p.posz))
                    worklist.push_back(p);
                p.posx = itr->posx + 1;
                p.posy = itr->posy;
                if (helpers::check_borders(&p.posx, &p.posy, &p.posz))
                    retlist->push_back(p);
                p.posx = itr->posx;
                p.posy = itr->posy - 1;
                if (helpers::check_borders(&p.posx, &p.posy, &p.posz))
                    worklist.push_back(p);
                p.posx = itr->posx;
                p.posy = itr->posy - 1;
                if (helpers::check_borders(&p.posx, &p.posy, &p.posz))
                    retlist->push_back(p);
                }

            }
            else
            {
                if(itr->posy > posy)
                {
                p.posx = itr->posx - 1;
                p.posy = itr->posy + 1;
                if (helpers::check_borders(&p.posx, &p.posy, &p.posz))
                    worklist.push_back(p);
                p.posx = itr->posx - 1;
                p.posy = itr->posy + 1;
                if (helpers::check_borders(&p.posx, &p.posy, &p.posz))
                    retlist->push_back(p);
                p.posx = itr->posx;
                p.posy = itr->posy + 1;
                if (helpers::check_borders(&p.posx, &p.posy, &p.posz))
                    worklist.push_back(p);
                p.posx = itr->posx;
                p.posy = itr->posy + 1;
                if (helpers::check_borders(&p.posx, &p.posy, &p.posz))
                    retlist->push_back(p);
                p.posx = itr->posx - 1;
                p.posy = itr->posy;
                if (helpers::check_borders(&p.posx, &p.posy, &p.posz))
                    worklist.push_back(p);
                p.posx = itr->posx - 1;
                p.posy = itr->posy;
                if (helpers::check_borders(&p.posx, &p.posy, &p.posz))
                    retlist->push_back(p);
                }
                else
                {
                p.posx = itr->posx - 1;
                p.posy = itr->posy - 1;
                if (helpers::check_borders(&p.posx, &p.posy, &p.posz))
                    worklist.push_back(p);
                p.posx = itr->posx - 1;
                p.posy = itr->posy - 1;
                if (helpers::check_borders(&p.posx, &p.posy, &p.posz))
                    retlist->push_back(p);
                p.posx = itr->posx - 1;
                p.posy = itr->posy;
                if (helpers::check_borders(&p.posx, &p.posy, &p.posz))
                    worklist.push_back(p);
                p.posx = itr->posx - 1;
                p.posy = itr->posy;
                if (helpers::check_borders(&p.posx, &p.posy, &p.posz))
                    retlist->push_back(p);
                p.posx = itr->posx;
                p.posy = itr->posy - 1;
                if (helpers::check_borders(&p.posx, &p.posy, &p.posz))
                    worklist.push_back(p);
                p.posx = itr->posx;
                p.posy = itr->posy - 1;
                if (helpers::check_borders(&p.posx, &p.posy, &p.posz))
                    retlist->push_back(p);
                }
            }

        worklist.erase(itr++);
    };
    if (retlist->begin()->posz == 0)
        return retlist;
    
    std::list<point> z_list;
    for (auto it = retlist->begin(); it != retlist->end(); ++it)
    {
        point p = *it;
        p.posz -= 1;
        z_list.push_back(p);
    }
    retlist->splice(retlist->begin(), z_list);

    return retlist;
}

void LOSfinder::clearLOS()
{
    worklist.clear();
}