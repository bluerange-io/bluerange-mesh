////////////////////////////////////////////////////////////////////////////////
// /****************************************************************************
// **
// ** Copyright (C) 2015-2021 M-Way Solutions GmbH
// ** Contact: https://www.blureange.io/licensing
// **
// ** This file is part of the Bluerange/FruityMesh implementation
// **
// ** $BR_BEGIN_LICENSE:GPL-EXCEPT$
// ** Commercial License Usage
// ** Licensees holding valid commercial Bluerange licenses may use this file in
// ** accordance with the commercial license agreement provided with the
// ** Software or, alternatively, in accordance with the terms contained in
// ** a written agreement between them and M-Way Solutions GmbH. 
// ** For licensing terms and conditions see https://www.bluerange.io/terms-conditions. For further
// ** information use the contact form at https://www.bluerange.io/contact.
// **
// ** GNU General Public License Usage
// ** Alternatively, this file may be used under the terms of the GNU
// ** General Public License version 3 as published by the Free Software
// ** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
// ** included in the packaging of this file. Please review the following
// ** information to ensure the GNU General Public License requirements will
// ** be met: https://www.gnu.org/licenses/gpl-3.0.html.
// **
// ** $BR_END_LICENSE$
// **
// ****************************************************************************/
////////////////////////////////////////////////////////////////////////////////
#include "gtest/gtest.h"
#include "MoveAnimation.h"
#include "Exceptions.h"
#include "CherrySimTester.h"
#include "CherrySimUtils.h"

TEST(TestMoveAnimation, TestFailedToStart) {
    //Tests that evaluating an unstarted animation throws an exception.
    MoveAnimation animation;
    animation.AddKeyPoint(MoveAnimationKeyPoint( 0,  1, 0, 1, MoveAnimationType::LERP));
    animation.AddKeyPoint(MoveAnimationKeyPoint( 1,  0, 0, 1, MoveAnimationType::LERP));
    animation.AddKeyPoint(MoveAnimationKeyPoint( 0, -1, 0, 1, MoveAnimationType::LERP));
    animation.AddKeyPoint(MoveAnimationKeyPoint(-1,  0, 0, 1, MoveAnimationType::LERP));
    Exceptions::DisableDebugBreakOnException disabler;
    ASSERT_THROW(animation.Evaluate(0), IllegalStateException);
}

TEST(TestMoveAnimation, TestAddAfterStartFail) {
    //Tests that adding new keypoints to an already running animation throws an exception.
    MoveAnimation animation;
    animation.AddKeyPoint(MoveAnimationKeyPoint(0, 1, 0, 1, MoveAnimationType::LERP));
    animation.Start(0, { 0, 0, 0 });
    Exceptions::DisableDebugBreakOnException disabler;
    ASSERT_THROW(animation.AddKeyPoint(MoveAnimationKeyPoint(1, 0, 0, 1, MoveAnimationType::LERP)), IllegalStateException);
}

TEST(TestMoveAnimation, TestZeroKeyPointsFail) {
    //Tests that starting an animation without keypoints throws an exception.
    MoveAnimation animation;
    Exceptions::DisableDebugBreakOnException disabler;
    ASSERT_THROW(animation.Start(0, { 0, 0, 0 }), IllegalStateException);
}

TEST(TestMoveAnimation, TestSimpleDiamondAnimation) {
    //Tests a simple animation that moves along a diamond shape (not looped)
    /*
    *    4. key point -> o
    *                   / 
    *                  /   
    * 3. key point -> o     o <- 1. key point
    *                  \   /
    *                   \ /
    *                    o <- 2. key point
    */
    MoveAnimation animation;
    animation.AddKeyPoint(MoveAnimationKeyPoint(0, 1, 0, 1, MoveAnimationType::LERP));
    animation.AddKeyPoint(MoveAnimationKeyPoint(1, 0, 0, 1, MoveAnimationType::LERP));
    animation.AddKeyPoint(MoveAnimationKeyPoint(0, -1, 0, 1, MoveAnimationType::LERP));
    animation.AddKeyPoint(MoveAnimationKeyPoint(-1, 0, 0, 1, MoveAnimationType::LERP));

    animation.Start(0, { 0, 0, 0 });
    ASSERT_TRUE(animation.IsStarted());
    ASSERT_FLOAT_EQ(animation.Evaluate(0).x, 0.0);
    ASSERT_FLOAT_EQ(animation.Evaluate(0).y, 0.0);
    ASSERT_FLOAT_EQ(animation.Evaluate(0).z, 0.0);

    ASSERT_FLOAT_EQ(animation.Evaluate(500).x, 0.0);
    ASSERT_FLOAT_EQ(animation.Evaluate(500).y, 0.5);
    ASSERT_FLOAT_EQ(animation.Evaluate(500).z, 0.0);

    ASSERT_FLOAT_EQ(animation.Evaluate(1000).x, 0.0);
    ASSERT_FLOAT_EQ(animation.Evaluate(1000).y, 1.0);
    ASSERT_FLOAT_EQ(animation.Evaluate(1000).z, 0.0);

    ASSERT_FLOAT_EQ(animation.Evaluate(1500).x, 0.5);
    ASSERT_FLOAT_EQ(animation.Evaluate(1500).y, 0.5);
    ASSERT_FLOAT_EQ(animation.Evaluate(1500).z, 0.0);

    ASSERT_FLOAT_EQ(animation.Evaluate(2000).x, 1.0);
    ASSERT_FLOAT_EQ(animation.Evaluate(2000).y, 0.0);
    ASSERT_FLOAT_EQ(animation.Evaluate(2000).z, 0.0);

    ASSERT_FLOAT_EQ(animation.Evaluate(2500).x,  0.5);
    ASSERT_FLOAT_EQ(animation.Evaluate(2500).y, -0.5);
    ASSERT_FLOAT_EQ(animation.Evaluate(2500).z,  0.0);

    ASSERT_FLOAT_EQ(animation.Evaluate(3000).x,  0.0);
    ASSERT_FLOAT_EQ(animation.Evaluate(3000).y, -1.0);
    ASSERT_FLOAT_EQ(animation.Evaluate(3000).z,  0.0);

    ASSERT_FLOAT_EQ(animation.Evaluate(3500).x, -0.5);
    ASSERT_FLOAT_EQ(animation.Evaluate(3500).y, -0.5);
    ASSERT_FLOAT_EQ(animation.Evaluate(3500).z,  0.0);

    ASSERT_TRUE(animation.IsStarted());

    auto lastPoint = animation.Evaluate(4000);

    ASSERT_FALSE(animation.IsStarted());
    ASSERT_FLOAT_EQ(lastPoint.x, -1.0);
    ASSERT_FLOAT_EQ(lastPoint.y,  0.0);
    ASSERT_FLOAT_EQ(lastPoint.z,  0.0);

    //After the last time has been picked, the animation should stop
    Exceptions::DisableDebugBreakOnException disabler;
    ASSERT_THROW(animation.Evaluate(4000), IllegalStateException);
}

TEST(TestMoveAnimation, TestSimpleDiamondAnimationCosine) {
    //Tests a simple animation that moves along a diamond shape (not looped, with cosine types)
    /*
    *    4. key point -> o
    *                   / 
    *                  /   
    * 3. key point -> o     o <- 1. key point
    *                  \   /
    *                   \ /
    *                    o <- 2. key point
    */
    MoveAnimation animation;
    animation.AddKeyPoint(MoveAnimationKeyPoint(0, 1, 0, 1, MoveAnimationType::COSINE));
    animation.AddKeyPoint(MoveAnimationKeyPoint(1, 0, 0, 1, MoveAnimationType::COSINE));
    animation.AddKeyPoint(MoveAnimationKeyPoint(0, -1, 0, 1, MoveAnimationType::COSINE));
    animation.AddKeyPoint(MoveAnimationKeyPoint(-1, 0, 0, 1, MoveAnimationType::COSINE));

    animation.Start(0, { 0, 0, 0 });
    ASSERT_TRUE(animation.IsStarted());
    constexpr float allowableError = 0.01f;
    ASSERT_NEAR(animation.Evaluate(0).x, 0.0, allowableError);
    ASSERT_NEAR(animation.Evaluate(0).y, 0.0, allowableError);
    ASSERT_NEAR(animation.Evaluate(0).z, 0.0, allowableError);

    ASSERT_NEAR(animation.Evaluate(250).x, 0.0,  allowableError);
    ASSERT_NEAR(animation.Evaluate(250).y, 0.15, allowableError);
    ASSERT_NEAR(animation.Evaluate(250).z, 0.0,  allowableError);

    ASSERT_NEAR(animation.Evaluate(500).x, 0.0, allowableError);
    ASSERT_NEAR(animation.Evaluate(500).y, 0.5, allowableError);
    ASSERT_NEAR(animation.Evaluate(500).z, 0.0, allowableError);

    ASSERT_NEAR(animation.Evaluate(1000).x, 0.0, allowableError);
    ASSERT_NEAR(animation.Evaluate(1000).y, 1.0, allowableError);
    ASSERT_NEAR(animation.Evaluate(1000).z, 0.0, allowableError);

    ASSERT_NEAR(animation.Evaluate(1500).x, 0.5, allowableError);
    ASSERT_NEAR(animation.Evaluate(1500).y, 0.5, allowableError);
    ASSERT_NEAR(animation.Evaluate(1500).z, 0.0, allowableError);

    ASSERT_NEAR(animation.Evaluate(2000).x, 1.0, allowableError);
    ASSERT_NEAR(animation.Evaluate(2000).y, 0.0, allowableError);
    ASSERT_NEAR(animation.Evaluate(2000).z, 0.0, allowableError);

    ASSERT_NEAR(animation.Evaluate(2500).x,  0.5, allowableError);
    ASSERT_NEAR(animation.Evaluate(2500).y, -0.5, allowableError);
    ASSERT_NEAR(animation.Evaluate(2500).z,  0.0, allowableError);

    ASSERT_NEAR(animation.Evaluate(3000).x,  0.0, allowableError);
    ASSERT_NEAR(animation.Evaluate(3000).y, -1.0, allowableError);
    ASSERT_NEAR(animation.Evaluate(3000).z,  0.0, allowableError);

    ASSERT_NEAR(animation.Evaluate(3500).x, -0.5, allowableError);
    ASSERT_NEAR(animation.Evaluate(3500).y, -0.5, allowableError);
    ASSERT_NEAR(animation.Evaluate(3500).z,  0.0, allowableError);

    ASSERT_TRUE(animation.IsStarted());

    auto lastPoint = animation.Evaluate(4000);

    ASSERT_FALSE(animation.IsStarted());
    ASSERT_NEAR(lastPoint.x, -1.0, allowableError);
    ASSERT_NEAR(lastPoint.y,  0.0, allowableError);
    ASSERT_NEAR(lastPoint.z,  0.0, allowableError);

    //After the last time has been picked, the animation should stop
    Exceptions::DisableDebugBreakOnException disabler;
    ASSERT_THROW(animation.Evaluate(4000), IllegalStateException);
}

TEST(TestMoveAnimation, TestSimpleDiamondAnimationBoolean) {
    //Tests a simple animation that moves along a diamond shape (not looped, with boolean types)
    /*
    *    4. key point -> o
    *                   / 
    *                  /   
    * 3. key point -> o     o <- 1. key point
    *                  \   /
    *                   \ /
    *                    o <- 2. key point
    */
    MoveAnimation animation;
    animation.AddKeyPoint(MoveAnimationKeyPoint(0, 1, 0, 1, MoveAnimationType::BOOLEAN));
    animation.AddKeyPoint(MoveAnimationKeyPoint(1, 0, 0, 1, MoveAnimationType::BOOLEAN));
    animation.AddKeyPoint(MoveAnimationKeyPoint(0, -1, 0, 1, MoveAnimationType::BOOLEAN));
    animation.AddKeyPoint(MoveAnimationKeyPoint(-1, 0, 0, 1, MoveAnimationType::BOOLEAN));

    animation.Start(0, { 0, 0, 0 });
    ASSERT_TRUE(animation.IsStarted());
    ASSERT_FLOAT_EQ(animation.Evaluate(0).x, 0.0);
    ASSERT_FLOAT_EQ(animation.Evaluate(0).y, 0.0);
    ASSERT_FLOAT_EQ(animation.Evaluate(0).z, 0.0);

    ASSERT_FLOAT_EQ(animation.Evaluate(400).x, 0.0);
    ASSERT_FLOAT_EQ(animation.Evaluate(400).y, 0.0);
    ASSERT_FLOAT_EQ(animation.Evaluate(400).z, 0.0);

    ASSERT_FLOAT_EQ(animation.Evaluate(1000).x, 0.0);
    ASSERT_FLOAT_EQ(animation.Evaluate(1000).y, 1.0);
    ASSERT_FLOAT_EQ(animation.Evaluate(1000).z, 0.0);

    ASSERT_FLOAT_EQ(animation.Evaluate(1400).x, 0.0);
    ASSERT_FLOAT_EQ(animation.Evaluate(1400).y, 1.0);
    ASSERT_FLOAT_EQ(animation.Evaluate(1400).z, 0.0);

    ASSERT_FLOAT_EQ(animation.Evaluate(2000).x, 1.0);
    ASSERT_FLOAT_EQ(animation.Evaluate(2000).y, 0.0);
    ASSERT_FLOAT_EQ(animation.Evaluate(2000).z, 0.0);

    ASSERT_FLOAT_EQ(animation.Evaluate(2400).x, 1.0);
    ASSERT_FLOAT_EQ(animation.Evaluate(2400).y, 0.0);
    ASSERT_FLOAT_EQ(animation.Evaluate(2400).z, 0.0);

    ASSERT_FLOAT_EQ(animation.Evaluate(3000).x,  0.0);
    ASSERT_FLOAT_EQ(animation.Evaluate(3000).y, -1.0);
    ASSERT_FLOAT_EQ(animation.Evaluate(3000).z,  0.0);

    ASSERT_FLOAT_EQ(animation.Evaluate(3400).x,  0.0);
    ASSERT_FLOAT_EQ(animation.Evaluate(3400).y, -1.0);
    ASSERT_FLOAT_EQ(animation.Evaluate(3400).z,  0.0);

    ASSERT_TRUE(animation.IsStarted());

    auto lastPoint = animation.Evaluate(4000);

    ASSERT_FALSE(animation.IsStarted());
    ASSERT_FLOAT_EQ(lastPoint.x, -1.0);
    ASSERT_FLOAT_EQ(lastPoint.y,  0.0);
    ASSERT_FLOAT_EQ(lastPoint.z,  0.0);

    //After the last time has been picked, the animation should stop
    Exceptions::DisableDebugBreakOnException disabler;
    ASSERT_THROW(animation.Evaluate(4000), IllegalStateException);
}

TEST(TestMoveAnimation, TestSimpleDiamondAnimationLooped) {
    //Tests a simple animation that moves along a diamond shape (looped)
    /*
    *    4. key point -> o
    *                   / \
    *                  /   \
    * 3. key point -> o     o <- 1. key point
    *                  \   /
    *                   \ /
    *                    o <- 2. key point
    */
    MoveAnimation animation;
    animation.SetLooped(true);
    animation.AddKeyPoint(MoveAnimationKeyPoint( 0,  1, 0, 1, MoveAnimationType::LERP));
    animation.AddKeyPoint(MoveAnimationKeyPoint( 1,  0, 0, 1, MoveAnimationType::LERP));
    animation.AddKeyPoint(MoveAnimationKeyPoint( 0, -1, 0, 1, MoveAnimationType::LERP));
    animation.AddKeyPoint(MoveAnimationKeyPoint(-1,  0, 0, 1, MoveAnimationType::LERP));

    animation.Start(0, { 0, 0, 0 });
    ASSERT_FLOAT_EQ(animation.Evaluate(   0).x, 0.0);
    ASSERT_FLOAT_EQ(animation.Evaluate(   0).y, 0.0);
    ASSERT_FLOAT_EQ(animation.Evaluate(   0).z, 0.0);

    ASSERT_FLOAT_EQ(animation.Evaluate( 500).x, 0.0);
    ASSERT_FLOAT_EQ(animation.Evaluate( 500).y, 0.5);
    ASSERT_FLOAT_EQ(animation.Evaluate( 500).z, 0.0);

    for (int i = 0; i < 10; i++)
    {
        ASSERT_FLOAT_EQ(animation.Evaluate(1000 + i * 4000).x, 0.0);
        ASSERT_FLOAT_EQ(animation.Evaluate(1000 + i * 4000).y, 1.0);
        ASSERT_FLOAT_EQ(animation.Evaluate(1000 + i * 4000).z, 0.0);

        ASSERT_FLOAT_EQ(animation.Evaluate(1500 + i * 4000).x, 0.5);
        ASSERT_FLOAT_EQ(animation.Evaluate(1500 + i * 4000).y, 0.5);
        ASSERT_FLOAT_EQ(animation.Evaluate(1500 + i * 4000).z, 0.0);

        ASSERT_FLOAT_EQ(animation.Evaluate(2000 + i * 4000).x, 1.0);
        ASSERT_FLOAT_EQ(animation.Evaluate(2000 + i * 4000).y, 0.0);
        ASSERT_FLOAT_EQ(animation.Evaluate(2000 + i * 4000).z, 0.0);

        ASSERT_FLOAT_EQ(animation.Evaluate(2500 + i * 4000).x, 0.5);
        ASSERT_FLOAT_EQ(animation.Evaluate(2500 + i * 4000).y, -0.5);
        ASSERT_FLOAT_EQ(animation.Evaluate(2500 + i * 4000).z, 0.0);

        ASSERT_FLOAT_EQ(animation.Evaluate(3000 + i * 4000).x, 0.0);
        ASSERT_FLOAT_EQ(animation.Evaluate(3000 + i * 4000).y, -1.0);
        ASSERT_FLOAT_EQ(animation.Evaluate(3000 + i * 4000).z, 0.0);

        ASSERT_FLOAT_EQ(animation.Evaluate(3500 + i * 4000).x, -0.5);
        ASSERT_FLOAT_EQ(animation.Evaluate(3500 + i * 4000).y, -0.5);
        ASSERT_FLOAT_EQ(animation.Evaluate(3500 + i * 4000).z, 0.0);

        ASSERT_FLOAT_EQ(animation.Evaluate(4000 + i * 4000).x, -1.0);
        ASSERT_FLOAT_EQ(animation.Evaluate(4000 + i * 4000).y, 0.0);
        ASSERT_FLOAT_EQ(animation.Evaluate(4000 + i * 4000).z, 0.0);

        ASSERT_FLOAT_EQ(animation.Evaluate(4500 + i * 4000).x, -0.5);
        ASSERT_FLOAT_EQ(animation.Evaluate(4500 + i * 4000).y, 0.5);
        ASSERT_FLOAT_EQ(animation.Evaluate(4500 + i * 4000).z, 0.0);
    }
}

#ifndef GITHUB_RELEASE
TEST(TestMoveAnimation, TestBuildUpViaCommands) {
    //Builds up animations via Terminal Commands and starts them.
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.nodeConfigName.insert({ "prod_mesh_nrf52", 2 });
    simConfig.simTickDurationMs = 10; //Test highly depends on simulation time per tick, so better be explicit.
    simConfig.preDefinedPositions = { {0, 0}, {0, 0} };
    //simConfig.verbose = true;

    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();

    tester.SimulateUntilClusteringDone(10 * 1000); //Give nodes some time to boot.

    ASSERT_FLOAT_EQ(tester.sim->nodes[0].x, 0);
    ASSERT_FLOAT_EQ(tester.sim->nodes[0].y, 0);
    ASSERT_FLOAT_EQ(tester.sim->nodes[0].z, 0);

    ASSERT_FLOAT_EQ(tester.sim->nodes[1].x, 0);
    ASSERT_FLOAT_EQ(tester.sim->nodes[1].y, 0);
    ASSERT_FLOAT_EQ(tester.sim->nodes[1].z, 0);

    //Test that non-existing animations are reported as such...
    tester.SendTerminalCommand(1, "sim animation exists my_test_animation");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"type\":\"sim_animation_exists\",\"name\":\"my_test_animation\",\"exists\":false}");

    //... and existing ones are reported as existing.
    tester.SendTerminalCommand(1, "sim animation create my_test_animation");
    tester.SendTerminalCommand(1, "sim animation exists my_test_animation");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"type\":\"sim_animation_exists\",\"name\":\"my_test_animation\",\"exists\":true}");

    //After the animation is removed, it must be reported as no longer existing.
    tester.SendTerminalCommand(1, "sim animation remove my_test_animation");
    tester.SendTerminalCommand(1, "sim animation exists my_test_animation");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"type\":\"sim_animation_exists\",\"name\":\"my_test_animation\",\"exists\":false}");

    tester.SendTerminalCommand(1, "sim animation create my_test_animation");
    tester.SendTerminalCommand(1, "sim animation add_keypoint my_test_animation 1 1 0 1");
    tester.SimulateGivenNumberOfSteps(2);

    //The animation was not started yet, it should have no effect on the nodes.
    ASSERT_FLOAT_EQ(tester.sim->nodes[0].x, 0);
    ASSERT_FLOAT_EQ(tester.sim->nodes[0].y, 0);
    ASSERT_FLOAT_EQ(tester.sim->nodes[0].z, 0);

    ASSERT_FLOAT_EQ(tester.sim->nodes[1].x, 0);
    ASSERT_FLOAT_EQ(tester.sim->nodes[1].y, 0);
    ASSERT_FLOAT_EQ(tester.sim->nodes[1].z, 0);

    //Start the animation and test for correct position updates.
    tester.SendTerminalCommand(1, "sim animation start BBBBB my_test_animation");
    for (int i = 0; i <= 100; i++)
    {
        tester.SimulateGivenNumberOfSteps(1);
        ASSERT_FLOAT_EQ(tester.sim->nodes[0].x, (float)i * 0.01f);
        ASSERT_FLOAT_EQ(tester.sim->nodes[0].y, (float)i * 0.01f);
        ASSERT_FLOAT_EQ(tester.sim->nodes[0].z, 0);

        //Starting the animation on one node should have no effect on another node.
        ASSERT_FLOAT_EQ(tester.sim->nodes[1].x, 0);
        ASSERT_FLOAT_EQ(tester.sim->nodes[1].y, 0);
        ASSERT_FLOAT_EQ(tester.sim->nodes[1].z, 0);
    }


    for (int i = 0; i <= 100; i++)
    {
        //After the animation is complete, make sure that the node stays where it is.
        tester.SimulateGivenNumberOfSteps(1);
        ASSERT_FLOAT_EQ(tester.sim->nodes[0].x, 1);
        ASSERT_FLOAT_EQ(tester.sim->nodes[0].y, 1);
        ASSERT_FLOAT_EQ(tester.sim->nodes[0].z, 0);
    }

    // Repeat the animation with another node to test that it works with different serial numbers.
    tester.SendTerminalCommand(1, "sim animation start BBBBC my_test_animation");
    for (int i = 0; i <= 100; i++)
    {
        tester.SimulateGivenNumberOfSteps(1);
        ASSERT_FLOAT_EQ(tester.sim->nodes[0].x, 1);
        ASSERT_FLOAT_EQ(tester.sim->nodes[0].y, 1);
        ASSERT_FLOAT_EQ(tester.sim->nodes[0].z, 0);

        ASSERT_FLOAT_EQ(tester.sim->nodes[1].x, (float)i * 0.01f);
        ASSERT_FLOAT_EQ(tester.sim->nodes[1].y, (float)i * 0.01f);
        ASSERT_FLOAT_EQ(tester.sim->nodes[1].z, 0);
    }
    for (int i = 0; i <= 100; i++)
    {
        tester.SimulateGivenNumberOfSteps(1);
        ASSERT_FLOAT_EQ(tester.sim->nodes[1].x, 1);
        ASSERT_FLOAT_EQ(tester.sim->nodes[1].y, 1);
        ASSERT_FLOAT_EQ(tester.sim->nodes[1].z, 0);
    }

    //Get name and check for animation of node that has no animation...
    tester.SendTerminalCommand(1, "sim animation get_name BBBBB");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"type\":\"sim_animation_get_name\",\"serial\":\"BBBBB\",\"name\":\"NULL\"}");
    tester.SendTerminalCommand(1, "sim animation is_running BBBBB");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"type\":\"sim_animation_is_running\",\"serial\":\"BBBBB\",\"code\":0}");

    //...check it again with applied animation...
    tester.SendTerminalCommand(1, "sim animation start BBBBB my_test_animation");
    tester.SimulateGivenNumberOfSteps(1);
    tester.SendTerminalCommand(1, "sim animation get_name BBBBB");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"type\":\"sim_animation_get_name\",\"serial\":\"BBBBB\",\"name\":\"my_test_animation\"}");
    tester.SendTerminalCommand(1, "sim animation is_running BBBBB");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"type\":\"sim_animation_is_running\",\"serial\":\"BBBBB\",\"code\":1}");
    tester.SendTerminalCommand(1, "sim animation get_name BBBBC");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"type\":\"sim_animation_get_name\",\"serial\":\"BBBBC\",\"name\":\"NULL\"}");
    tester.SendTerminalCommand(1, "sim animation is_running BBBBC");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"type\":\"sim_animation_is_running\",\"serial\":\"BBBBC\",\"code\":0}");

    //...and stop the animation.
    tester.SendTerminalCommand(1, "sim animation start BBBBC my_test_animation");
    tester.SendTerminalCommand(1, "sim animation stop BBBBB");
    tester.SendTerminalCommand(1, "sim animation get_name BBBBB");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"type\":\"sim_animation_get_name\",\"serial\":\"BBBBB\",\"name\":\"NULL\"}");
    tester.SendTerminalCommand(1, "sim animation is_running BBBBB");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"type\":\"sim_animation_is_running\",\"serial\":\"BBBBB\",\"code\":0}");
    tester.SendTerminalCommand(1, "sim animation get_name BBBBC");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"type\":\"sim_animation_get_name\",\"serial\":\"BBBBC\",\"name\":\"my_test_animation\"}");
    tester.SendTerminalCommand(1, "sim animation is_running BBBBC");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"type\":\"sim_animation_is_running\",\"serial\":\"BBBBC\",\"code\":1}");

    //Test that a second animation works as well
    tester.SendTerminalCommand(1, "sim animation create my_test_animation2");
    tester.SendTerminalCommand(1, "sim animation add_keypoint my_test_animation2 2 2 0 1");
    tester.SimulateGivenNumberOfSteps(2);
    tester.SendTerminalCommand(1, "sim animation start BBBBB my_test_animation2");
    ASSERT_FLOAT_EQ(tester.sim->nodes[0].x, 1);
    ASSERT_FLOAT_EQ(tester.sim->nodes[0].y, 1);
    ASSERT_FLOAT_EQ(tester.sim->nodes[0].z, 0);
    for (int i = 0; i <= 100; i++)
    {
        tester.SimulateGivenNumberOfSteps(1);
        ASSERT_FLOAT_EQ(tester.sim->nodes[0].x, 1.0f + (float)i * 0.01f);
        ASSERT_FLOAT_EQ(tester.sim->nodes[0].y, 1.0f + (float)i * 0.01f);
        ASSERT_FLOAT_EQ(tester.sim->nodes[0].z, 0);
    }
    for (int i = 0; i <= 100; i++)
    {
        tester.SimulateGivenNumberOfSteps(1);
        ASSERT_FLOAT_EQ(tester.sim->nodes[0].x, 2);
        ASSERT_FLOAT_EQ(tester.sim->nodes[0].y, 2);
        ASSERT_FLOAT_EQ(tester.sim->nodes[0].z, 0);
    }

    //Test looped animations
    tester.SendTerminalCommand(1, "sim animation create my_looped_animation");
    tester.SendTerminalCommand(1, "sim animation add_keypoint my_looped_animation 3 2 0 1");
    tester.SendTerminalCommand(1, "sim animation add_keypoint my_looped_animation 3 3 0 1");
    tester.SendTerminalCommand(1, "sim animation add_keypoint my_looped_animation 2 3 0 1");
    tester.SendTerminalCommand(1, "sim animation add_keypoint my_looped_animation 2 2 0 1");
    tester.SendTerminalCommand(1, "sim animation set_looped my_looped_animation 1");
    tester.SimulateGivenNumberOfSteps(10);

    tester.SendTerminalCommand(1, "sim animation start BBBBB my_looped_animation");
    tester.SimulateGivenNumberOfSteps(1);
    for (int loopRepeats = 0; loopRepeats < 10; loopRepeats++)
    {
        ASSERT_FLOAT_EQ(tester.sim->nodes[0].x, 2);
        ASSERT_FLOAT_EQ(tester.sim->nodes[0].y, 2);
        ASSERT_FLOAT_EQ(tester.sim->nodes[0].z, 0);
        tester.SimulateForGivenTime(500);
        ASSERT_FLOAT_EQ(tester.sim->nodes[0].x, 2.5f);
        ASSERT_FLOAT_EQ(tester.sim->nodes[0].y, 2);
        ASSERT_FLOAT_EQ(tester.sim->nodes[0].z, 0);
        tester.SimulateForGivenTime(500);
        ASSERT_FLOAT_EQ(tester.sim->nodes[0].x, 3);
        ASSERT_FLOAT_EQ(tester.sim->nodes[0].y, 2);
        ASSERT_FLOAT_EQ(tester.sim->nodes[0].z, 0);
        tester.SimulateForGivenTime(500);
        ASSERT_FLOAT_EQ(tester.sim->nodes[0].x, 3);
        ASSERT_FLOAT_EQ(tester.sim->nodes[0].y, 2.5f);
        ASSERT_FLOAT_EQ(tester.sim->nodes[0].z, 0);
        tester.SimulateForGivenTime(500);
        ASSERT_FLOAT_EQ(tester.sim->nodes[0].x, 3);
        ASSERT_FLOAT_EQ(tester.sim->nodes[0].y, 3);
        ASSERT_FLOAT_EQ(tester.sim->nodes[0].z, 0);
        tester.SimulateForGivenTime(500);
        ASSERT_FLOAT_EQ(tester.sim->nodes[0].x, 2.5f);
        ASSERT_FLOAT_EQ(tester.sim->nodes[0].y, 3);
        ASSERT_FLOAT_EQ(tester.sim->nodes[0].z, 0);
        tester.SimulateForGivenTime(500);
        ASSERT_FLOAT_EQ(tester.sim->nodes[0].x, 2);
        ASSERT_FLOAT_EQ(tester.sim->nodes[0].y, 3);
        ASSERT_FLOAT_EQ(tester.sim->nodes[0].z, 0);
        tester.SimulateForGivenTime(500);
        ASSERT_FLOAT_EQ(tester.sim->nodes[0].x, 2);
        ASSERT_FLOAT_EQ(tester.sim->nodes[0].y, 2.5f);
        ASSERT_FLOAT_EQ(tester.sim->nodes[0].z, 0);
        tester.SimulateForGivenTime(500);
    }

    //Make sure that setting positions cancels animations
    tester.SendTerminalCommand(1, "sim animation is_running BBBBB");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"type\":\"sim_animation_is_running\",\"serial\":\"BBBBB\",\"code\":1}");
    tester.SendTerminalCommand(1, "sim set_position BBBBB 0.5 0.5 0.5");
    tester.SendTerminalCommand(1, "sim animation is_running BBBBB");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"type\":\"sim_animation_is_running\",\"serial\":\"BBBBB\",\"code\":0}");
}
#endif

#ifndef GITHUB_RELEASE
TEST(TestMoveAnimation, TestLoadFromFile)
{
    //Loads animations from a JSON files and checks that everything was loaded and played properly.
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.nodeConfigName.insert({ "prod_mesh_nrf52", 2 });
    simConfig.simTickDurationMs = 10; //Test highly depends on simulation time per tick, so better be explicit.
    simConfig.preDefinedPositions = { {0, 0}, {0, 0} };
    //simConfig.verbose = true;

    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();

    tester.SimulateUntilClusteringDone(10 * 1000); //Give nodes some time to boot.

    const std::string path = "/test/res/MoveAnimation.json";
    tester.SendTerminalCommand(1, "sim animation load_path %s", path.c_str());
    tester.SimulateGivenNumberOfSteps(1);

    ASSERT_EQ      (tester.sim->loadedMoveAnimations.size(), 2);
    ASSERT_EQ      (tester.sim->loadedMoveAnimations["first-test-animation"].defaultAnimationType, MoveAnimationType::LERP);
    ASSERT_EQ      (tester.sim->loadedMoveAnimations["first-test-animation"].looped, true);
    ASSERT_EQ      (tester.sim->loadedMoveAnimations["first-test-animation"].keyPoints.size(), 3);
    ASSERT_EQ      (tester.sim->loadedMoveAnimations["first-test-animation"].keyPoints[0].type, MoveAnimationType::LERP);
    ASSERT_FLOAT_EQ(tester.sim->loadedMoveAnimations["first-test-animation"].keyPoints[0].x, 1.0);
    ASSERT_FLOAT_EQ(tester.sim->loadedMoveAnimations["first-test-animation"].keyPoints[0].y, 0.1);
    ASSERT_FLOAT_EQ(tester.sim->loadedMoveAnimations["first-test-animation"].keyPoints[0].z, -3);
    ASSERT_FLOAT_EQ(tester.sim->loadedMoveAnimations["first-test-animation"].keyPoints[0].duration, 1.5);
    ASSERT_EQ      (tester.sim->loadedMoveAnimations["first-test-animation"].keyPoints[1].type, MoveAnimationType::COSINE);
    ASSERT_FLOAT_EQ(tester.sim->loadedMoveAnimations["first-test-animation"].keyPoints[1].x, -3.0);
    ASSERT_FLOAT_EQ(tester.sim->loadedMoveAnimations["first-test-animation"].keyPoints[1].y, 0.7);
    ASSERT_FLOAT_EQ(tester.sim->loadedMoveAnimations["first-test-animation"].keyPoints[1].z, -2);
    ASSERT_FLOAT_EQ(tester.sim->loadedMoveAnimations["first-test-animation"].keyPoints[1].duration, 1.3);
    ASSERT_EQ      (tester.sim->loadedMoveAnimations["first-test-animation"].keyPoints[2].type, MoveAnimationType::LERP);
    ASSERT_FLOAT_EQ(tester.sim->loadedMoveAnimations["first-test-animation"].keyPoints[2].x, 0.123);
    ASSERT_FLOAT_EQ(tester.sim->loadedMoveAnimations["first-test-animation"].keyPoints[2].y, 0.456);
    ASSERT_FLOAT_EQ(tester.sim->loadedMoveAnimations["first-test-animation"].keyPoints[2].z, 0.789);
    ASSERT_FLOAT_EQ(tester.sim->loadedMoveAnimations["first-test-animation"].keyPoints[2].duration, 0.1);
    
    ASSERT_EQ      (tester.sim->loadedMoveAnimations["second-test-animation"].defaultAnimationType, MoveAnimationType::COSINE);
    ASSERT_EQ      (tester.sim->loadedMoveAnimations["second-test-animation"].looped, false);
    ASSERT_EQ      (tester.sim->loadedMoveAnimations["second-test-animation"].keyPoints.size(), 4);
    ASSERT_EQ      (tester.sim->loadedMoveAnimations["second-test-animation"].keyPoints[0].type, MoveAnimationType::BOOLEAN);
    ASSERT_FLOAT_EQ(tester.sim->loadedMoveAnimations["second-test-animation"].keyPoints[0].x, 1.3);
    ASSERT_FLOAT_EQ(tester.sim->loadedMoveAnimations["second-test-animation"].keyPoints[0].y, 0.2);
    ASSERT_FLOAT_EQ(tester.sim->loadedMoveAnimations["second-test-animation"].keyPoints[0].z, -2);
    ASSERT_FLOAT_EQ(tester.sim->loadedMoveAnimations["second-test-animation"].keyPoints[0].duration, 2.5);
    ASSERT_EQ      (tester.sim->loadedMoveAnimations["second-test-animation"].keyPoints[1].type, MoveAnimationType::COSINE);
    ASSERT_FLOAT_EQ(tester.sim->loadedMoveAnimations["second-test-animation"].keyPoints[1].x, -2.0);
    ASSERT_FLOAT_EQ(tester.sim->loadedMoveAnimations["second-test-animation"].keyPoints[1].y, 0.1);
    ASSERT_FLOAT_EQ(tester.sim->loadedMoveAnimations["second-test-animation"].keyPoints[1].z, -1);
    ASSERT_FLOAT_EQ(tester.sim->loadedMoveAnimations["second-test-animation"].keyPoints[1].duration, 1.2);
    ASSERT_EQ      (tester.sim->loadedMoveAnimations["second-test-animation"].keyPoints[2].type, MoveAnimationType::COSINE);
    ASSERT_FLOAT_EQ(tester.sim->loadedMoveAnimations["second-test-animation"].keyPoints[2].x, -0.123);
    ASSERT_FLOAT_EQ(tester.sim->loadedMoveAnimations["second-test-animation"].keyPoints[2].y, -0.456);
    ASSERT_FLOAT_EQ(tester.sim->loadedMoveAnimations["second-test-animation"].keyPoints[2].z, -0.789);
    ASSERT_FLOAT_EQ(tester.sim->loadedMoveAnimations["second-test-animation"].keyPoints[2].duration, 0.5);
    ASSERT_EQ      (tester.sim->loadedMoveAnimations["second-test-animation"].keyPoints[3].type, MoveAnimationType::COSINE);
    ASSERT_FLOAT_EQ(tester.sim->loadedMoveAnimations["second-test-animation"].keyPoints[3].x, 0.0);
    ASSERT_FLOAT_EQ(tester.sim->loadedMoveAnimations["second-test-animation"].keyPoints[3].y, 0.0);
    ASSERT_FLOAT_EQ(tester.sim->loadedMoveAnimations["second-test-animation"].keyPoints[3].z, 0.0);
    ASSERT_FLOAT_EQ(tester.sim->loadedMoveAnimations["second-test-animation"].keyPoints[3].duration, 0.5);


    tester.SendTerminalCommand(1, "sim animation start BBBBB first-test-animation");
    tester.SimulateGivenNumberOfSteps(1);

    ASSERT_FLOAT_EQ(tester.sim->nodes[0].x, 0);
    ASSERT_FLOAT_EQ(tester.sim->nodes[0].y, 0);
    ASSERT_FLOAT_EQ(tester.sim->nodes[0].z, 0);

    constexpr float acceptableError = 0.1f;

    for (int repeats = 0; repeats < 3; repeats++)
    {
        tester.SimulateForGivenTime(1500);
        ASSERT_NEAR(tester.sim->nodes[0].x,  1.0f, acceptableError);
        ASSERT_NEAR(tester.sim->nodes[0].y,  0.1f, acceptableError);
        ASSERT_NEAR(tester.sim->nodes[0].z, -3.0f, acceptableError);

        tester.SimulateForGivenTime(1300);
        ASSERT_NEAR(tester.sim->nodes[0].x, -3.0f, acceptableError);
        ASSERT_NEAR(tester.sim->nodes[0].y,  0.7f, acceptableError);
        ASSERT_NEAR(tester.sim->nodes[0].z, -2.0f, acceptableError);

        tester.SimulateForGivenTime(100);
        ASSERT_NEAR(tester.sim->nodes[0].x, 0.123f, acceptableError);
        ASSERT_NEAR(tester.sim->nodes[0].y, 0.456f, acceptableError);
        ASSERT_NEAR(tester.sim->nodes[0].z, 0.789f, acceptableError);
    }


    tester.SendTerminalCommand(1, "sim animation start BBBBC second-test-animation");
    tester.SimulateGivenNumberOfSteps(1);

    ASSERT_FLOAT_EQ(tester.sim->nodes[1].x, 0);
    ASSERT_FLOAT_EQ(tester.sim->nodes[1].y, 0);
    ASSERT_FLOAT_EQ(tester.sim->nodes[1].z, 0);

    
    tester.SimulateForGivenTime(2500);
    ASSERT_NEAR(tester.sim->nodes[1].x, 1.3, acceptableError);
    ASSERT_NEAR(tester.sim->nodes[1].y, 0.2, acceptableError);
    ASSERT_NEAR(tester.sim->nodes[1].z, -2, acceptableError);

    tester.SimulateForGivenTime(1200);
    ASSERT_NEAR(tester.sim->nodes[1].x, -2.0, acceptableError);
    ASSERT_NEAR(tester.sim->nodes[1].y, 0.1, acceptableError);
    ASSERT_NEAR(tester.sim->nodes[1].z, -1, acceptableError);

    tester.SimulateForGivenTime(500);
    ASSERT_NEAR(tester.sim->nodes[1].x, -0.123, acceptableError);
    ASSERT_NEAR(tester.sim->nodes[1].y, -0.456, acceptableError);
    ASSERT_NEAR(tester.sim->nodes[1].z, -0.789, acceptableError);

    tester.SimulateForGivenTime(500);
    ASSERT_NEAR(tester.sim->nodes[1].x, 0.0, acceptableError);
    ASSERT_NEAR(tester.sim->nodes[1].y, 0.0, acceptableError);
    ASSERT_NEAR(tester.sim->nodes[1].z, 0.0, acceptableError);

    for (int i = 0; i < 2 * 1000; i++)
    {
        tester.SimulateForGivenTime(1);
        ASSERT_NEAR(tester.sim->nodes[1].x, 0.0, acceptableError);
        ASSERT_NEAR(tester.sim->nodes[1].y, 0.0, acceptableError);
        ASSERT_NEAR(tester.sim->nodes[1].z, 0.0, acceptableError);
    }
}
#endif
