/******************************************************************************
*                 SOFA, Simulation Open-Framework Architecture                *
*                    (c) 2006 INRIA, USTL, UJF, CNRS, MGH                     *
*                                                                             *
* This program is free software; you can redistribute it and/or modify it     *
* under the terms of the GNU Lesser General Public License as published by    *
* the Free Software Foundation; either version 2.1 of the License, or (at     *
* your option) any later version.                                             *
*                                                                             *
* This program is distributed in the hope that it will be useful, but WITHOUT *
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or       *
* FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License *
* for more details.                                                           *
*                                                                             *
* You should have received a copy of the GNU Lesser General Public License    *
* along with this program. If not, see <http://www.gnu.org/licenses/>.        *
*******************************************************************************
*                       Plugin SoftRobots.Inverse                             *
*                                                                             *
* This plugin is distributed under the GNU AGPL v3 (Affero General            *
* Public License) license.                                                    *
*                                                                             *
* Authors: Christian Duriez, Eulalie Coevoet, Yinoussa Adagolodjo             *
*                                                                             *
* (c) 2023 INRIA                                                              *
*                                                                             *
* Contact information: https://project.inria.fr/softrobot/contact/            *
******************************************************************************/
#pragma once

#include <SoftRobots.Inverse/component/constraint/ForcePointActuator.h>
#include <sofa/core/visual/VisualParams.h>
#include <math.h>

namespace softrobotsinverse::constraint
{
using sofa::helper::ReadAccessor;
using sofa::helper::WriteAccessor;
using sofa::type::Mat;
using sofa::type::Vec3;
using sofa::type::vector;
using sofa::linearalgebra::BaseVector;
using sofa::core::VecCoordId ;
using sofa::helper::rabs;

template<class DataTypes>
ForcePointActuator<DataTypes>::ForcePointActuator(MechanicalState* object)
    : Inherit1(object)

    , d_indices(initData(&d_indices, "indices",
                       "Index of the point of the model on which we want to apply the force"))

    , d_maxForce(initData(&d_maxForce, "maxForce",
                       ""))

    , d_minForce(initData(&d_minForce, "minForce",
                       ""))

    , d_initForce(initData(&d_initForce, Real(0.0), "initForce",
                       "Initial force if any. Default is 0."))

    , d_maxForceVariation(initData(&d_maxForceVariation, "maxForceVariation",
                       "Only available if the direction is set."))

    , d_force(initData(&d_force, "force",
                       "Warning: to get the actual force you should divide this value by dt."))

    , d_displacement(initData(&d_displacement, Real(0.0), "displacement",
                       ""))

    , d_direction(initData(&d_direction, "direction",
                           "Direction of the force we want to apply. If d=[0,0,0], the direction \n"
                           "will be optimized."))

    , d_epsilon(initData(&d_epsilon, Real(1e-3), "penalty",
                           "Use this value to prioritize the constraint. 0 means no limitation on the energy \n"
                            "transfered by this actuator. Default is 1e-3."))

    , d_showForce(initData(&d_showForce, false, "showForce",
                           ""))

    , d_visuScale(initData(&d_visuScale, Real(0.1), "visuScale",
                           ""))

{
    setUpData();
}


template<class DataTypes>
void ForcePointActuator<DataTypes>::setUpData()
{
    d_force.setReadOnly(true);
    d_displacement.setReadOnly(true);

    d_showForce.setGroup("Visualization");
    d_visuScale.setGroup("Visualization");
}


template<class DataTypes>
ForcePointActuator<DataTypes>::~ForcePointActuator()
{
}


template<class DataTypes>
void ForcePointActuator<DataTypes>::init()
{
    Inherit1::init();

    if(m_state==nullptr)
        msg_error(this) << "There is no mechanical state associated with this node. "
                            "the object is deactivated. "
                            "To remove this error message fix your scene possibly by "
                            "adding a MechanicalObject." ;

    initData();
    initLimit();
}


template<class DataTypes>
void ForcePointActuator<DataTypes>::reinit()
{
    initData();
    initLimit();
}

template<class DataTypes>
void ForcePointActuator<DataTypes>::initData()
{
    m_dim = (d_direction.getValue().norm()<1e-10)? Deriv::total_size: 1;

    if(d_epsilon.isSet())
    {
        m_hasEpsilon = true;
        m_epsilon = d_epsilon.getValue();
    }

    if(d_initForce.isSet())
    {
        m_hasLambdaInit = true;
        m_lambdaInit[0] = d_initForce.getValue();
    }

    sofa::type::vector<Real> force;
    force.resize(m_dim, d_initForce.getValue());
    d_force.setValue(force);

    // QP on only one value, we set dimension to one
    m_lambdaInit.resize(m_dim);
    m_lambdaMax.resize(m_dim);
    m_lambdaMin.resize(m_dim);
}


template<class DataTypes>
void ForcePointActuator<DataTypes>::initLimit()
{
    if(d_maxForce.isSet())
        m_hasLambdaMax = true;

    if(d_minForce.isSet())
        m_hasLambdaMin = true;

    if(d_maxForceVariation.isSet())
    {
        m_hasLambdaMax = true;
        m_hasLambdaMin = true;
    }

    updateLimit();
}


template<class DataTypes>
void ForcePointActuator<DataTypes>::updateLimit()
{
    if(d_maxForce.isSet())
    {
        for (auto& lambda: m_lambdaMax)
            lambda = d_maxForce.getValue();
    }

    if(d_minForce.isSet())
    {
        for (auto& lambda: m_lambdaMin)
            lambda = d_minForce.getValue();
    }

    if(d_maxForceVariation.isSet())
    {
        if (m_dim>1)
        {
            for(unsigned int j=0; j<Deriv::total_size; j++)
            {
                if(rabs(m_lambdaMin[j] - d_force.getValue()[j]) >= d_maxForceVariation.getValue() || !d_minForce.isSet())
                    m_lambdaMin[j] = d_force.getValue()[j] - d_maxForceVariation.getValue();
                if(rabs(m_lambdaMax[j] - d_force.getValue()[j]) >= d_maxForceVariation.getValue() || !d_maxForce.isSet())
                    m_lambdaMax[j] = d_force.getValue()[j] + d_maxForceVariation.getValue();
            }
        }
        else
        {
            if(rabs(m_lambdaMin[0] - d_force.getValue()[0]) >= d_maxForceVariation.getValue() || !d_minForce.isSet())
                m_lambdaMin[0] = d_force.getValue()[0] - d_maxForceVariation.getValue();
            if(rabs(m_lambdaMax[0] - d_force.getValue()[0]) >= d_maxForceVariation.getValue() || !d_maxForce.isSet())
                m_lambdaMax[0] = d_force.getValue()[0] + d_maxForceVariation.getValue();
        }
    }
}


template<class DataTypes>
void ForcePointActuator<DataTypes>::buildConstraintMatrix(const ConstraintParams* cParams,
                                                          DataMatrixDeriv &cMatrix,
                                                          unsigned int &cIndex,
                                                          const DataVecCoord &x)
{
    SOFA_UNUSED(cParams);
    SOFA_UNUSED(x);

    d_constraintIndex.setValue(cIndex);
    const auto& constraintIndex = sofa::helper::getReadAccessor(d_constraintIndex);
    const auto& nbIndices = d_indices.getValue().size();

    Deriv direction = d_direction.getValue();

    if(m_dim > 1) // No fixed direction
    {
        MatrixDeriv& matrix = *cMatrix.beginEdit();

        for(unsigned int j=0; j<Deriv::total_size; j++)
        {
            Deriv dir;
            dir[j] = 1;
            MatrixDerivRowIterator rowIterator = matrix.writeLine(constraintIndex+j);
            for(unsigned int i=0; i<nbIndices; i++)
                if(d_indices.getValue()[i]<m_state->getSize())
                    rowIterator.addCol(d_indices.getValue()[i], dir);
            cIndex++;
        }
    }
    else
    {
        direction /= direction.norm();
        MatrixDeriv& matrix = *cMatrix.beginEdit();
        MatrixDerivRowIterator rowIterator = matrix.writeLine(constraintIndex);
        for(unsigned int i=0; i<nbIndices; i++)
            if(d_indices.getValue()[i]<m_state->getSize())
                rowIterator.addCol(d_indices.getValue()[i], direction);

        cIndex++;
    }

    cMatrix.endEdit();
    
    m_nbLines = cIndex - constraintIndex;
}


template<class DataTypes>
void ForcePointActuator<DataTypes>::getConstraintViolation(const ConstraintParams* cParams,
                                                           BaseVector *resV,
                                                           const BaseVector *Jdx)
{
    SOFA_UNUSED(cParams);
    SOFA_UNUSED(Jdx);

    const auto& constraintId = sofa::helper::getReadAccessor(d_constraintIndex);

    if(m_dim > 1) // No fixed direction
    {
        for(unsigned int j=0; j<Deriv::total_size; j++)
            resV->set(constraintId+j, 0.);
    }
    else
        resV->set(constraintId, 0.);
}


template<class DataTypes>
void ForcePointActuator<DataTypes>::storeResults(vector<double> &lambda, vector<double> &delta)
{
    WriteAccessor<sofa::Data<vector<Real>>> force = d_force;

    d_displacement.setValue(delta[0]);

    if(m_dim > 1) // No fixed direction
    {
        for(unsigned int j=0; j<Deriv::total_size; j++)
            force[j]=lambda[j];
    }
    else
        force[0] = lambda[0];

    updateLimit();

    Actuator<DataTypes>::storeResults(lambda, delta);
}


template<class DataTypes>
void ForcePointActuator<DataTypes>::draw(const VisualParams* vparams)
{
    if (!vparams->displayFlags().getShowInteractionForceFields() || !d_showForce.getValue())
        return;

    vparams->drawTool()->setLightingEnabled(true);

    ReadAccessor<sofa::Data<sofa::type::vector<sofa::Index>>> indices = sofa::helper::getReadAccessor(d_indices);
    ReadAccessor<sofa::Data<Real>> visuScale = sofa::helper::getReadAccessor(d_visuScale);
    ReadAccessor<sofa::Data<VecCoord> > positions = m_state->readPositions();
    ReadAccessor<sofa::Data<vector<Real>>> force = d_force;
    Deriv direction = d_direction.getValue();

    static const sofa::type::RGBAColor color(0,0,0.8,1);
    for(const sofa::Index& index: indices)
    {
        if(index < m_state->getSize())
        {
            Vec3 position = Vec3(positions[index][0], positions[index][1], positions[index][2]);
            if(m_dim > 1)
            {
                Vec3 dir = Vec3(force[0], force[1], force[2]);
                vparams->drawTool()->drawArrow(position - dir*visuScale, position, dir.norm()*visuScale/20.0f, color, 4);
            }
            else
            {
                direction /= direction.norm();
                Vec3 dir = Vec3(direction[0], direction[1], direction[2]);
                vparams->drawTool()->drawArrow(position - dir*log(force[0]+1)*visuScale, position, log(force[0]+1)*visuScale/20.0f, color, 4);
            }
        }
    }

    vparams->drawTool()->restoreLastState();
}


} // namespace

