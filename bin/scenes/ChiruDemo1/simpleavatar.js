// A simple walking avatar with physics & third person camera
var rotate_speed = 150.0;
var mouse_rotate_sensitivity = 0.3;
var move_force = 15.0;
var fly_speed_factor = 0.25;
var damping_force = 3.0;
var walk_anim_speed = 0.5;
var avatar_camera_distance = 7.0;
var avatar_camera_height = 1.0;
var avatar_mass = 10;

// Tracking motion with entity actions
var motion_x = 0;
var motion_y = 0;
var motion_z = 0;
var rotate = 0;

// Needed bools for logic
var isserver = server.IsRunning();
var own_avatar = false;
var flying = false;
var falling = false;
var fish_created = false;
var tripod = false;

// Animation detection
var standAnimName = "Stand";
var walkAnimName = "Walk";
var flyAnimName = "Fly";
var hoverAnimName = "Hover";
var sitAnimName = "SitOnGround";
var waveAnimName = "Wave";
var animsDetected = false;
var listenGesture = false;

// Create avatar on server, and camera & inputmapper on client
if (isserver) {
    ServerInitialize();
} else {
    ClientInitialize();
}

function ServerInitialize() {   
    var avatar = me.GetOrCreateComponent("EC_Avatar");
    var rigidbody = me.GetOrCreateComponent("EC_RigidBody");

    // Set the avatar appearance. This creates the mesh & animationcontroller, once the avatar asset has loaded
    // Note: for now, you need the default_avatar.xml in your bin/data/assets folder
    avatar.appearanceId = "http://chiru.cie.fi/avatarassets/default_avatar.xml"

    // Set physics properties
    var sizeVec = new float3(0.5, 0.5, 2.4);
    rigidbody.mass = avatar_mass;
    rigidbody.shapeType = 3; // Capsule
    rigidbody.size = sizeVec;
    
    rigidbody.angularFactor = float3.zero; // Set zero angular factor so that body stays upright

    // Hook to physics update
    rigidbody.GetPhysicsWorld().Updated.connect(ServerUpdatePhysics);

    // Hook to tick update for continuous rotation update
    frame.Updated.connect(ServerUpdate);

    // Connect actions
    me.Action("Move").Triggered.connect(ServerHandleMove);
    me.Action("Stop").Triggered.connect(ServerHandleStop);
    me.Action("ToggleFly").Triggered.connect(ServerHandleToggleFly);
    me.Action("Rotate").Triggered.connect(ServerHandleRotate);
    me.Action("StopRotate").Triggered.connect(ServerHandleStopRotate);
    me.Action("MouseLookX").Triggered.connect(ServerHandleMouseLookX);
    me.Action("Gesture").Triggered.connect(ServerHandleGesture);
    
    rigidbody.PhysicsCollision.connect(ServerHandleCollision);
}

function ServerUpdate(frametime) {
    if (!animsDetected) {
        CommonFindAnimations();
    }
        
    if (rotate != 0) {
        var rotateVec = new float3(0,0,-rotate_speed * rotate * frametime);
        me.rigidbody.Rotate(rotateVec);
    }

    CommonUpdateAnimation(frametime);
}

function ServerHandleCollision(ent, pos, normal, distance, impulse, newCollision) {
    if (falling && newCollision) {
        falling = false;
        ServerSetAnimationState();
    }
}

function ServerUpdatePhysics(frametime) {
    var placeable = me.placeable;
    var rigidbody = me.rigidbody;

    if (!flying) {
        // Apply motion force
        // If diagonal motion, normalize
        if ((motion_x != 0) || (motion_y != 0)) {
            var mag = 1.0 / Math.sqrt(motion_x * motion_x + motion_y * motion_y);
            var impulseVec = new float3(mag * move_force * motion_x, -mag * move_force * motion_y, 0);
            impulseVec = placeable.GetRelativeVector(impulseVec);
            rigidbody.ApplyImpulse(impulseVec);
        }

        // Apply damping. Only do this if the body is active, because otherwise applying forces
        // to a resting object wakes it up
        if (rigidbody.IsActive()) {
            var dampingVec = rigidbody.GetLinearVelocity();
            dampingVec.x = -damping_force * dampingVec.x;
            dampingVec.y = -damping_force * dampingVec.y;
            dampingVec.z = 0;
            // Jump and wait for us to 
            // come down before allowing new jump
            if (motion_z == 1 && !falling) {
                dampingVec.z = 75;
                motion_z = 0;
                falling = true;
            }
            rigidbody.ApplyImpulse(dampingVec);
        }
    } else {
        // Manually move the avatar placeable when flying
        // this has the downside of no collisions.
        // Feel free to reimplement properly with mass enabled.       
        var av_placeable = me.placeable;
        var av_transform = av_placeable.transform;
        
        // Make a vector where we have moved
        var moveVec = new float3();
        moveVec.x = motion_x * fly_speed_factor;
        moveVec.y = -motion_y * fly_speed_factor;
        moveVec.z = motion_z * fly_speed_factor;
        
        // Apply that with av looking direction to the current position
        var offsetVec = av_placeable.GetRelativeVector(moveVec);
        av_transform.pos.x = av_transform.pos.x + offsetVec.x;
        av_transform.pos.y = av_transform.pos.y + offsetVec.y;
        av_transform.pos.z = av_transform.pos.z + offsetVec.z;
        
        // This may look confusing. Its kind of a hack to tilt the avatar 
        // when flying to the sides when you turn with A and D.
        // At the same time we need to lift up the Z of the av accorting to the angle of tilt
        if (motion_x != 0) {
            if (motion_y > 0 && av_transform.rot.x <= 5) {
                av_transform.rot.x = av_transform.rot.x + motion_y/2;
	    }
            if (motion_y < 0 && av_transform.rot.x >= -5) {
                av_transform.rot.x = av_transform.rot.x + motion_y/2;
	    }
            if (motion_y != 0 && av_transform.rot.x > 0) {
                av_transform.pos.z = av_transform.pos.z + (av_transform.rot.x * 0.0045); // magic number
	    }
	    if (motion_y != 0 && av_transform.rot.x < 0) {
                av_transform.pos.z = av_transform.pos.z + (-av_transform.rot.x * 0.0045); // magic number
	    }
        }
        if (motion_y == 0 && av_transform.rot.x > 0) {
            av_transform.rot.x = av_transform.rot.x - 0.5;
	}
	if (motion_y == 0 && av_transform.rot.x < 0) {
            av_transform.rot.x = av_transform.rot.x + 0.5;
	}

        av_placeable.transform = av_transform;
    }
}

function ServerHandleMove(param) {
    // It is possible to query from whom the action did come from
    //var sender = server.GetActionSender();
    //if (sender)
    //    print("Move action from " + sender.GetName());

    if (param == "forward") {
        motion_x = 1;
    }
    if (param == "back") {
        motion_x = -1;
    }
    if (param == "right") {
        motion_y = 1;
    }
    if (param == "left") {
        motion_y = -1;
    }
    if (param == "up") {
        motion_z = 1;
    }
    if (param == "down") {
        motion_z = -1;
    }

    ServerSetAnimationState();
}

function ServerHandleStop(param) {
    if ((param == "forward") && (motion_x == 1)) {
        motion_x = 0;
    }
    if ((param == "back") && (motion_x == -1)) {
        motion_x = 0;
    }
    if ((param == "right") && (motion_y == 1)) {
        motion_y = 0;
    }
    if ((param == "left") && (motion_y == -1)) {
        motion_y = 0;
    }
    if ((param == "up") && (motion_z == 1)) {
        motion_z = 0;
    }
    if ((param == "down") && (motion_z == -1)) {
        motion_z = 0;
    }
        
    ServerSetAnimationState();
}

function ServerHandleToggleFly() {
    var rigidbody = me.GetOrCreateComponent("EC_RigidBody");
    flying = !flying;
    if (flying) {
        rigidbody.mass = 0;
    } else {
        // Reset the x rot if left
        var av_placeable = me.placeable;
        var av_transform = av_placeable.transform;
        if (av_transform.rot.x != 0) {
            av_transform.rot.x = 0;
            av_placeable.transform = av_transform;
        }
        
        // Set mass back for collisions
        rigidbody.mass = avatar_mass;
        // Push avatar a bit to the fly direction
        // so the motion does not just stop to a wall
        var rigidbody = me.rigidbody;
        var moveVec = new float3();
        moveVec.x = motion_x * 120;
        moveVec.y = -motion_y * 120;
        moveVec.z = motion_z * 120;
        var pushVec = av_placeable.GetRelativeVector(moveVec);
        rigidbody.ApplyImpulse(pushVec);
    }
    ServerSetAnimationState();
}

function ServerHandleRotate(param) {
    if (param == "left") {
        rotate = -1;
    }
    if (param == "right") {
        rotate = 1;
    }
}

function ServerHandleStopRotate(param) {
    if ((param == "left") && (rotate == -1)) {
        rotate = 0;
    }
    if ((param == "right") && (rotate == 1)) {
        rotate = 0;
    }
}

function ServerHandleMouseLookX(param) {
    var move = parseInt(param);
    var rotateVec = new float3(0,0,-mouse_rotate_sensitivity * move);
    me.rigidbody.Rotate(rotateVec);
}

function ServerHandleGesture(gestureName) {
    var animName = "";
    if (gestureName == "wave") {
        animName = waveAnimName;
    }
    if (animName == "") {
        return;
    }
        
    // Update the variable to sync to client if changed
    var animcontroller = me.animationcontroller;
    if (animcontroller != null) {
        if (animcontroller.animationState != animName) {
            animcontroller.animationState = animName;
	}
    }
}

function ServerSetAnimationState() {
    // Not flying: Stand, Walk or Crouch
    var animName = standAnimName;
    if ((motion_x != 0) || (motion_y != 0)) {
        animName = walkAnimName;
    } else if (motion_z == -1 && !falling) {
        animName = sitAnimName;
    }
        
    // Flying: Fly if moving in x-axis, otherwise hover
    if (flying || falling) {
        animName = flyAnimName;
        if (motion_x == 0)
            animName = hoverAnimName;
    }
    
    if (animName == "") {
        return;
    }

    // Update the variable to sync to client if changed
    var animcontroller = me.animationcontroller;
    if (animcontroller != null) {
        if (animcontroller.animationState != animName) {
            animcontroller.animationState = animName;
	}
    }
}

function ClientInitialize() {
    // Check if this is our own avatar
    // Note: bad security. For now there's no checking who is allowed to invoke actions
    // on an entity, and we could theoretically control anyone's avatar
    if (me.GetName() == "Avatar" + client.GetConnectionID()) {
        own_avatar = true;
        ClientCreateInputMapper();
        ClientCreateAvatarCamera();
        
        me.Action("MouseScroll").Triggered.connect(ClientHandleMouseScroll);
        me.Action("Zoom").Triggered.connect(ClientHandleKeyboardZoom);
        me.Action("ToggleTripod").Triggered.connect(ClientHandleToggleTripod);
        me.Action("MouseLookX").Triggered.connect(ClientHandleTripodLookX);
        me.Action("MouseLookY").Triggered.connect(ClientHandleTripodLookY);
    }
    else
    {
        // Make hovering name tag for other clients
        clientName = me.GetComponent("EC_Name");
        if (clientName != null) {
            // Description holds the actual login name
            if (clientName.description != "") {
                var hoveringWidget = me.GetOrCreateComponent("EC_HoveringWidget", 2, false);
                if (hoveringWidget != null) {
                    hoveringWidget.SetNetworkSyncEnabled(false);
                    hoveringWidget.SetTemporary(true);
                    hoveringWidget.InitializeBillboards();
                    hoveringWidget.SetButtonsDisabled(true);
                    hoveringWidget.SetText(clientName.description);
                    hoveringWidget.SetFontSize(100);
                    hoveringWidget.SetTextHeight(200);
                    hoveringWidget.Show();
                }
            }
        }
    }

    // Hook to tick update to update visual effects (both own and others' avatars)
    frame.Updated.connect(ClientUpdate);
}

function IsCameraActive()
{
    var cameraentity = scene.GetEntityByName("AvatarCamera");
    if (cameraentity == null)
        return false;
    var camera = cameraentity.camera;
    return camera.IsActive();
}

function ClientHandleToggleTripod()
{
    var cameraentity = scene.GetEntityByName("AvatarCamera");
    if (cameraentity == null)
        return;

    var camera = cameraentity.camera;
    if (camera.IsActive() == false)
    {
        tripod = false;
        return;
    }

    if (tripod == false)
        tripod = true;
    else
        tripod = false;
}

function ClientHandleTripodLookX(param)
{
    if (tripod)
    {
        var cameraentity = scene.GetEntityByName("AvatarCamera");
        if (cameraentity == null)
            return;
        var cameraplaceable = cameraentity.placeable;
        var cameratransform = cameraplaceable.transform;

        var move = parseInt(param);
        cameratransform.rot.z -= mouse_rotate_sensitivity * move;
        cameraplaceable.transform = cameratransform;
    }
}

function ClientHandleTripodLookY(param)
{
    if (tripod)
    {
        var cameraentity = scene.GetEntityByName("AvatarCamera");
        if (cameraentity == null)
            return;
        var cameraplaceable = cameraentity.placeable;
        var cameratransform = cameraplaceable.transform;

        var move = parseInt(param);
        cameratransform.rot.x -= mouse_rotate_sensitivity * move;
        cameraplaceable.transform = cameratransform;
    }
}

function ClientUpdate(frametime)
{
    // Tie enabled state of inputmapper to the enabled state of avatar camera
    if (own_avatar) {
        var avatarcameraentity = scene.GetEntityByName("AvatarCamera");
        var inputmapper = me.inputmapper;
        if ((avatarcameraentity != null) && (inputmapper != null)) {
            var active = avatarcameraentity.camera.IsActive();
            if (inputmapper.enabled != active) {
                inputmapper.enabled = active;
	    }
        }
        ClientUpdateAvatarCamera(frametime);
    }

    if (!animsDetected) {
        CommonFindAnimations();
    }
    CommonUpdateAnimation(frametime);
    
    // Uncomment this to attach a fish to the avatar's head
    //if (!fish_created)
    //    CreateFish();
}

function ClientCreateInputMapper() {
    // Create a nonsynced inputmapper
    var inputmapper = me.GetOrCreateComponent("EC_InputMapper", 2, false);
    inputmapper.contextPriority = 101;
    inputmapper.takeMouseEventsOverQt = true;
    inputmapper.takeKeyboardEventsOverQt = true;
    inputmapper.modifiersEnabled = false;
    inputmapper.keyrepeatTrigger = false; // Disable repeat keyevent sending over network, not needed and will flood network
    inputmapper.executionType = 2; // Execute actions on server
    inputmapper.RegisterMapping("W", "Move(forward)", 1);
    inputmapper.RegisterMapping("S", "Move(back)", 1);
    inputmapper.RegisterMapping("A", "Move(left)", 1);
    inputmapper.RegisterMapping("D", "Move(right))", 1);
    inputmapper.RegisterMapping("Up", "Move(forward)", 1);
    inputmapper.RegisterMapping("Down", "Move(back)", 1);
    inputmapper.RegisterMapping("Left", "Rotate(left)", 1);
    inputmapper.RegisterMapping("Right", "Rotate(right))", 1);
    inputmapper.RegisterMapping("F", "ToggleFly()", 1);
    inputmapper.RegisterMapping("Q", "Gesture(wave)", 1);
    inputmapper.RegisterMapping("Space", "Move(up)", 1);
    inputmapper.RegisterMapping("C", "Move(down)", 1);
    inputmapper.RegisterMapping("W", "Stop(forward)", 3);
    inputmapper.RegisterMapping("S", "Stop(back)", 3);
    inputmapper.RegisterMapping("A", "Stop(left)", 3);
    inputmapper.RegisterMapping("D", "Stop(right)", 3);
    inputmapper.RegisterMapping("Up", "Stop(forward)", 3);
    inputmapper.RegisterMapping("Down", "Stop(back)", 3);
    inputmapper.RegisterMapping("Left", "StopRotate(left)", 3);
    inputmapper.RegisterMapping("Right", "StopRotate(right))", 3);
    inputmapper.RegisterMapping("Space", "Stop(up)", 3);
    inputmapper.RegisterMapping("C", "Stop(down)", 3);

    // Connect gestures
    inputContext = inputmapper.GetInputContext();
    inputContext.GestureStarted.connect(GestureStarted);
    inputContext.GestureUpdated.connect(GestureUpdated);
    
    // Local camera matter for mouse scroll
    var inputmapper = me.GetOrCreateComponent("EC_InputMapper", "CameraMapper", 2, false);
    inputmapper.SetNetworkSyncEnabled(false);
    inputmapper.contextPriority = 100;
    inputmapper.takeMouseEventsOverQt = true;
    inputmapper.modifiersEnabled = false;
    inputmapper.executionType = 1; // Execute actions locally
    inputmapper.RegisterMapping("T", "ToggleTripod", 1);
    inputmapper.RegisterMapping("+", "Zoom(in)", 1);
    inputmapper.RegisterMapping("-", "Zoom(out)", 1);
}

function ClientCreateAvatarCamera() {
    if (scene.GetEntityByName("AvatarCamera") != null) {
        return;
    }

    var cameraentity = scene.CreateEntityRaw(scene.NextFreeIdLocal());
    cameraentity.SetName("AvatarCamera");
    cameraentity.SetTemporary(true);

    var camera = cameraentity.GetOrCreateComponent("EC_Camera");
    var placeable = cameraentity.GetOrCreateComponent("EC_Placeable");

    camera.SetActive();

    // Set initial position
    ClientUpdateAvatarCamera();
}

function GestureStarted(gestureEvent)
{
    if (!IsCameraActive())
        return;
    if (gestureEvent.GestureType() == Qt.PanGesture)
    {
        listenGesture = true;
        var x = new Number(gestureEvent.Gesture().offset.toPoint().x());
        me.Exec(2, "MouseLookX", x.toString());
        gestureEvent.Accept();
    }
    else if (gestureEvent.GestureType() == Qt.PinchGesture)
        gestureEvent.Accept();
}

function GestureUpdated(gestureEvent)
{
    if (!IsCameraActive())
        return;
   
    if (gestureEvent.GestureType() == Qt.PanGesture && listenGesture == true)
    {
        // Rotate avatar with X pan gesture
        delta = gestureEvent.Gesture().delta.toPoint();
        var x = new Number(delta.x());
        me.Exec(2, "MouseLookX", x.toString());
        
        // Start walking or stop if total Y len of pan gesture is 100
        var walking = false;
        if (me.animationcontroller.animationState == walkAnimName)
            walking = true;
        var totalOffset = gestureEvent.Gesture().offset.toPoint();
        if (totalOffset.y() < -100) 
        {
            if (walking) {
                me.Exec(2, "Stop", "forward");
                me.Exec(2, "Stop", "back");
            } else
                me.Exec(2, "Move", "forward");
            listenGesture = false;
        }
        else if (totalOffset.y() > 100) 
        {
            if (walking) {
                me.Exec(2, "Stop", "forward");
                me.Exec(2, "Stop", "back");
            } else
                me.Exec(2, "Move", "back");
            listenGesture = false;
        }
        gestureEvent.Accept();
    }
    else if (gestureEvent.GestureType() == Qt.PinchGesture)
    {
        var scaleChange = gestureEvent.Gesture().scaleFactor - gestureEvent.Gesture().lastScaleFactor;
        if (scaleChange > 0.1 || scaleChange < -0.1)
            ClientHandleMouseScroll(scaleChange * 100);
        gestureEvent.Accept();
    }
}

function ClientHandleKeyboardZoom(direction) {
    if (direction == "in") {
        ClientHandleMouseScroll(10);
    } else if (direction == "out") {
        ClientHandleMouseScroll(-10);
    }
}

function ClientHandleMouseScroll(relativeScroll)
{
    if (!IsCameraActive())
        return;
    var moveAmount = 0;
    if (relativeScroll < 0 && avatar_camera_distance < 500) {
        if (relativeScroll < -50)
            moveAmount = 2;
        else
            moveAmount = 1;
    } else if (relativeScroll > 0 && avatar_camera_distance > 1) {
        if (relativeScroll > 50)
            moveAmount = -2
        else
            moveAmount = -1;
    }
    if (moveAmount != 0)
    {
        // Add movement
        avatar_camera_distance = avatar_camera_distance + moveAmount;
        // Clamp distance  to be between 1 and 500
        if (avatar_camera_distance < 1)
            avatar_camera_distance = 1;
        else if (avatar_camera_distance > 500)
            avatar_camera_distance = 500;
    }
}

function ClientUpdateAvatarCamera() {
    if (!tripod)
    {
        var cameraentity = scene.GetEntityByName("AvatarCamera");
        if (cameraentity == null)
            return;
        var cameraplaceable = cameraentity.placeable;
        var avatarplaceable = me.placeable;

        var cameratransform = cameraplaceable.transform;
        var avatartransform = avatarplaceable.transform;
        var offsetVec = new float3(-avatar_camera_distance,0,avatar_camera_height);
        offsetVec = avatarplaceable.GetRelativeVector(offsetVec);
        cameratransform.pos.x = avatartransform.pos.x + offsetVec.x;
        cameratransform.pos.y = avatartransform.pos.y + offsetVec.y;
        cameratransform.pos.z = avatartransform.pos.z + offsetVec.z;
        // Note: this is not nice how we have to fudge the camera rotation to get it to show the right things
        cameratransform.rot.x = 90;
        cameratransform.rot.z = avatartransform.rot.z - 90;

        cameraplaceable.transform = cameratransform;
    }
}

function CommonFindAnimations() {
    var animcontrol = me.animationcontroller;
    var availableAnimations = animcontrol.GetAvailableAnimations();
    if (availableAnimations.length > 0) {
        // Detect animation names
        var searchAnims = [standAnimName, walkAnimName, flyAnimName, hoverAnimName, sitAnimName, waveAnimName];
        for(var i=0; i<searchAnims.length; i++) {
            var animName = searchAnims[i];
            if (availableAnimations.indexOf(animName) == -1) {
                // Disable this animation by setting it to a empty string
                print("Could not find animation for:", animName, " - disabling animation");
                searchAnims[i] = ""; 
            }
        }
        
        // Assign the possible empty strings for 
        // not found anims back to the variables
        standAnimName = searchAnims[0];
        walkAnimName = searchAnims[1];
        flyAnimName = searchAnims[2];
        hoverAnimName = searchAnims[3];
        sitAnimName = searchAnims[4];
        
        animsDetected = true;
    }
}

function CommonUpdateAnimation(frametime) {   
    if (!animsDetected) {
        return;
    }
        
    var animcontroller = me.animationcontroller;
    var rigidbody = me.rigidbody;
    if ((animcontroller == null) || (rigidbody == null)) {
        return;
    }

    var animName = animcontroller.animationState;
            
    // Enable animation, skip with headless server
    if (animName != "" && !framework.IsHeadless()) {
        // Do custom speeds for certain anims
        if (animName == hoverAnimName) {
            animcontroller.SetAnimationSpeed(animName, 0.25);
	}
        if (animName == sitAnimName) { // Does not affect the anim speed on jack at least?!
            animcontroller.SetAnimationSpeed(animName, 0.5);
	}
        if (animName == waveAnimName) {
            animcontroller.SetAnimationSpeed(animName, 0.75);
	}
        // Enable animation
        if (!animcontroller.IsAnimationActive(animName)) {
            // Gestures with non exclusive
            if (animName == waveAnimName) {
                animcontroller.EnableAnimation(animName, false, 0.25, 0.25, false);
            // Normal anims exclude others
	    } else {
                animcontroller.EnableExclusiveAnimation(animName, true, 0.25, 0.25, false);
	    }
        }
    }
    
    // If walk animation is playing, adjust its speed according to the avatar rigidbody velocity
    if (animName != ""  && animcontroller.IsAnimationActive(walkAnimName)) {
        // Note: on client the rigidbody does not exist, so the velocity is only a replicated attribute
        var velocity = rigidbody.linearVelocity;
        var walkspeed = Math.sqrt(velocity.x * velocity.x + velocity.y * velocity.y) * walk_anim_speed;
        animcontroller.SetAnimationSpeed(walkAnimName, walkspeed);
    }
}
